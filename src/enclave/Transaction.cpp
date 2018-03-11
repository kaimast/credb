#include "op_info.h"
#include "Transaction.h"
#include "Ledger.h"
#include "Witness.h"
#include "util/keys.h"
#include "OpContext.h"

namespace credb
{
namespace trusted
{

Transaction::Transaction(IsolationLevel isolation, Ledger &ledger_, const OpContext &op_context_, LockHandle *lock_handle_)
    : m_isolation(isolation), ledger(ledger_), op_context(op_context_), lock_handle(ledger_, lock_handle_)
{
}

Transaction::Transaction(bitstream &request, Ledger &ledger_, const OpContext &op_context_)
    : ledger(ledger_), op_context(op_context_), lock_handle(ledger) 
{
    request >> reinterpret_cast<uint8_t&>(m_isolation) >> generate_witness;

    if(isolation_level() == IsolationLevel::Serializable)
    {    
        // FIXME: how to avoid phantom read while avoiding locking all shards?
        for(shard_id_t i = 0; i < NUM_SHARDS; ++i)
        {
            m_shard_lock_types[i] = LockType::Read;
        }
    }

    // construct op history and also collect shards_lock_type
    uint32_t ops_size = 0;
    request >> ops_size;

    for(uint32_t i = 0; i < ops_size; ++i)
    {
        auto op = new_operation_info_from_req(request);
        if(!op)
        {
            return;
        }

        register_operation(op);
    }
}

Transaction::~Transaction()
{
    clear();
}

void Transaction::clear()
{
    lock_handle.clear();

    if(!lock_handle.has_parent())
    {
        for(auto &kv : m_shard_lock_types)
        {
            ledger.organize_ledger(kv.first);
        }
    }

    for(auto op: m_ops)
    {
        delete op;
    }

    m_ops.clear();
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
                           const event_id_t &eid)
{
    auto [key, path] = parse_path(full_path);
    (void)path; //if eid hasn't changed value hasn't change either so no need to check path

    const LockType lock_type = m_shard_lock_types[sid];
    event_id_t latest_eid;

    obj = ledger.get_latest_version(op_context, collection, key, "", latest_eid, lock_handle, lock_type);

    if(!obj.valid() || latest_eid != eid)
    {
        error = "Key [" + key + "] reads outdated value";
        return false;
    }

    return true;
}

operation_info_t *Transaction::new_operation_info_from_req(bitstream &req)
{
    OperationType op;
    req >> reinterpret_cast<uint8_t &>(op);
    
    switch(op)
    {
    case OperationType::GetObject:
        return new get_info_t(*this, req);
    case OperationType::HasObject:
        return new has_obj_info_t(*this, req);
    case OperationType::CheckObject:
        return new check_obj_info_t(*this, req);
    case OperationType::PutObject:
        return new put_info_t(*this, req);
    case OperationType::AddToObject:
        return new add_info_t(*this, req);
    case OperationType::FindObjects:
        return new find_info_t(*this, req);
    case OperationType::RemoveObject:
        return new remove_info_t(*this, req);
    default:
        this->error = "Unknown OperationType " + std::to_string(static_cast<uint8_t>(op));
        log_error(this->error);
        return nullptr;
    }
}

void Transaction::register_operation(operation_info_t *op)
{
    m_ops.push_back(op);
    op->collect_shard_lock_type();
}

bool Transaction::phase_one()
{
    // first acquire locks for all pending shards to ensure atomicity
    for(auto &kv : m_shard_lock_types)
    {
        lock_handle.get_shard(kv.first, kv.second);
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

    // validate reads
    for(auto op : m_ops)
    {
        if(!op->validate())
        {
            return false;
        }
    }

    return true;
}

Witness Transaction::phase_two()
{
    std::unordered_set<event_id_t> read_set, write_set;
    std::array<uint16_t, NUM_SHARDS> write_shards;
    write_shards.fill(0);

    for(auto op : m_ops)
    {
        op->extract_reads(read_set);
        op->extract_writes(write_shards);
    }

    for(shard_id_t shard = 0; shard < write_shards.size(); ++shard)
    {
        auto num = write_shards[shard];

        if(num > 0)
        {
            ledger.get_next_event_ids(write_set, shard, num, &lock_handle);
        }
    }

    for(auto op : m_ops)
    {
        if(!op->do_write(read_set, write_set))
        {
            assert(!error.empty());
            throw std::runtime_error(error);
        }
    }

    Witness witness;
    
    if(generate_witness)
    {
        writer.end_array(); // operations
        writer.end_map(); // witness root
    }

    if(error.empty())
    {
        if(generate_witness)
        {
            json::Document doc = writer.make_document();
            witness.set_data(std::move(doc.data()));
            if(!sign_witness(ledger.m_enclave, witness))
            {
                error = "cannot sign witness";
            }
        }
    }

    bitstream bstream;
    if(!error.empty())
    {
        throw std::runtime_error(error);
    }

    return witness;
}

Witness Transaction::commit()
{
    if(!phase_one())
    {
        throw std::runtime_error(error);
    }

    return phase_two();
}


}
}
