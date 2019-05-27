/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "TransactionLedger.h"

namespace credb
{
namespace trusted
{

TransactionLedger::TransactionLedger(BufferManager &buffer_manager)
    : m_buffer_manager(buffer_manager), m_pending_block_id(INVALID_BLOCK)
{

    generate_block();
}

void TransactionLedger::organize_ledger()
{
    m_mutex.lock();

    auto pending = get_block(m_pending_block_id);

    // Wait until we have reached at least min block size
    if(pending->get_data_size() < MIN_BLOCK_SIZE)
    {
        m_mutex.unlock();
        return;
    }
    
    generate_block();
    auto newb = get_block(m_pending_block_id);
    m_mutex.unlock();

    pending->seal();
    pending->flush_page();
    newb->flush_page();
}

ledger_pos_t TransactionLedger::insert(const std::map<taskid_t, OpContext> &op_contexts, identity_uid_t tx_root, transaction_id_t tx_id, const op_set_t &local_ops, const std::map<identity_uid_t, op_set_t> &remote_ops)
{
    auto write_op_set = [] (json::Writer &writer, const op_set_t &set) -> void
    {
        writer.start_array();
        for(auto &entry: set.reads)
        {
            writer.write_integer(entry.shard);
            writer.write_integer(entry.block);
            writer.write_integer(entry.index);
        }
        writer.end_array();

        writer.start_array();
        for(auto &entry: set.writes)
        {
            writer.write_integer(entry.shard);
            writer.write_integer(entry.block);
            writer.write_integer(entry.index);
        }
        writer.end_array();
    };

    json::Writer writer;
    writer.start_array();

    writer.start_map();
    for(auto& [task, op_context] : op_contexts)
    {
        writer.write_string(std::to_string(task), op_context.to_string());
    }
    writer.end_map();

    writer.write_integer(tx_root);
    writer.write_integer(tx_id);
    
    writer.start_array();
    write_op_set(writer, local_ops);
    writer.end_array();

    writer.start_array();
    for(auto &[idty, op_set] : remote_ops)
    {
        writer.write_integer(idty);
        write_op_set(writer, op_set);
    }
    writer.end_array();
    writer.end_array();
 
    auto block = get_pending_block(LockType::Write);

    auto doc = writer.make_document();
    auto idx = block->insert(doc);

    ledger_pos_t pos = {block->identifier(), idx};
    m_num_pending_events = idx;

    if(block->get_data_size() >= MIN_BLOCK_SIZE)
    {
        organize_ledger();
    }

    block->write_unlock();
    return pos;
}

TransactionHandle TransactionLedger::get(ledger_pos_t pos)
{
    auto block = get_block(pos.block);
    block->read_lock();

    auto hdl = block->get(pos.index);

    block->read_unlock();
    return hdl;
}

}
}
