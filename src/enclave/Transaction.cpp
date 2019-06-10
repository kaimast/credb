/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "op_info.h"
#include "Transaction.h"
#include "Ledger.h"
#include "Witness.h"
#include "util/keys.h"
#include "OpContext.h"
#include "TransactionManager.h"

namespace credb::trusted
{

Transaction::Transaction(IsolationLevel isolation, Ledger &ledger_, TransactionLedger &transaction_ledger, TransactionManager &tx_mgr, identity_uid_t root, transaction_id_t id, bool is_remote)
    : ledger(ledger_),
      m_isolation(isolation), m_transaction_ledger(transaction_ledger),
      m_transaction_mgr(tx_mgr),
      m_lock_handle(ledger_, nullptr, true), // let's make them always non-blocking for now, it seems to be aproblem when threads are saturated
      m_root(root), m_identifier(id), m_is_remote(is_remote)
{
}

Transaction::~Transaction() = default;

void Transaction::init_task(taskid_t tid, const OpContext &context)
{
    m_op_contexts.emplace(tid, context.duplicate());
}

void Transaction::abort()
{
    if(m_state == TransactionState::Aborted)
    {
        // no-op
    }
    else if(m_state == TransactionState::Pending
          || m_state == TransactionState::Prepared)
    {
        m_state = TransactionState::Aborted;
        cleanup();
    }
    else
    {
        throw std::runtime_error("Cannot abort transaction: invalid state!");
    }
}

void Transaction::cleanup()
{
    if(!m_lock_handle.has_parent()
       && m_state == TransactionState::Committed)
    {
        for(auto &[shard, lock_type] : m_shard_lock_types)
        {
            if(lock_type == LockType::Write)
            {
                ledger.organize_ledger(shard);
            }
        }
    }

    for(auto op: m_ops)
    {
        delete op;
    }

    m_lock_handle.clear();
    m_ops.clear();
    m_transaction_mgr.remove_transaction(*this);
}

void Transaction::set_read_lock(shard_id_t sid)
{
    if(!m_shard_lock_types.count(sid))
    {
        m_shard_lock_types[sid] = LockType::Read;
    }
}

void Transaction::set_write_lock(shard_id_t sid)
{
    m_shard_lock_types[sid] = LockType::Write;
}

bool Transaction::check_repeatable_read(ObjectEventHandle &obj,
                           const std::string &collection,
                           const std::string &full_path,
                           shard_id_t sid,
                           const event_id_t &expected_eid,
                           taskid_t task)
{
    auto [key, path] = parse_path(full_path);
    (void)path; //if eid hasn't changed value hasn't change either so no need to check path

    const LockType lock_type = m_shard_lock_types[sid];
    event_id_t latest_eid;

    obj = ledger.get_latest_version(get_op_context(task), collection, key, "", latest_eid, m_lock_handle, lock_type);

    if(!obj.valid() || latest_eid != expected_eid)
    {
        set_error("Non-repeatable read: key [" + key + "] reads outdated value");
        return false;
    }

    return true;
}

void Transaction::register_operation(operation_info_t *op)
{
    m_ops.push_back(op);
    op->collect_shard_lock_type();
}

bool Transaction::prepare(bool generate_witness)
{
    if(m_state != TransactionState::Pending)
    {
        throw std::runtime_error("Cannot prepare: invalid state");
    }

    // first acquire locks for all pending shards to ensure atomicity
    try
    {
        for(auto &[shard_no, lock_type] : m_shard_lock_types)
        {
            m_lock_handle.get_shard(shard_no, lock_type);
        }
    }
    catch(const would_block_exception& e)
    {
        // We can't wait here as it may cause a deadlock
        set_error("Lock contention");
        this->abort();
        return false;
    }

    // witness root
    if(generate_witness)
    {
        writer.start_map();

        // TODO: timestamp
        
        switch(isolation_level())
        {
        case IsolationLevel::ReadCommitted:
            writer.write_string("isolation", "ReadCommitted");
            break;
        case IsolationLevel::RepeatableRead:
            writer.write_string("isolation", "RepeatableRead");
            break;
        case IsolationLevel::Serializable:
            writer.write_string("isolation", "Serializable");
            break;
        }
        writer.start_array(Witness::OP_FIELD_NAME);
    }

    bool result = true;

    // validate reads
    for(auto op : m_ops)
    {
        if(!op->validate(generate_witness))
        {
            result = false;
            break;
        }
    }

    if(!result)
    {
        abort();
    }
    else
    {
        m_state = TransactionState::Prepared;
    }

    return result;
}

Witness Transaction::commit(bool generate_witness)
{
    if(m_state != TransactionState::Prepared)
    {
        throw std::runtime_error("Cannot commit: invalid state");
    }

    std::set<event_id_t> read_set, write_set;
    std::array<uint16_t, NUM_SHARDS> write_shards;
    write_shards.fill(0);

    for(auto op : m_ops)
    {
        op->extract_reads(read_set);
        op->extract_writes(write_shards);
    }

    auto num_shards = static_cast<shard_id_t>(write_shards.size());
    for(shard_id_t shard = 0; shard < num_shards; ++shard)
    {
        auto num = write_shards[shard];

        if(num > 0)
        {
            ledger.get_next_event_ids(write_set, shard, num, &m_lock_handle);
        }
    }

    auto transaction_ref = m_transaction_ledger.insert(m_op_contexts, get_root(), identifier(), {read_set, write_set}, {});

    for(auto op : m_ops)
    {
        op->do_write(transaction_ref, generate_witness);
    }

    Witness witness;
    
    if(generate_witness)
    {
        writer.end_array(); // operations
        writer.end_map(); // witness root
    }

    // create witness
    if(generate_witness)
    {
        auto doc = writer.make_document();
        witness.set_data(std::move(doc.data()));

        auto res = sign_witness(ledger.m_enclave, witness);

        if(!res)
        {
            // TODO
            // This should only happen if the datastore is in an invalid state
            // It's probably good to shut down the enclave here.
        }
    }

    m_state = TransactionState::Committed;
    cleanup();
    return witness;
}

} // namespace credb::trusted
