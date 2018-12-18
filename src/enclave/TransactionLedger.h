/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <unordered_set>
#include <unordered_map>

#include "util/event_id_hash.h"
#include "ledger_pos.h"
#include "Block.h"
#include "OpContext.h"
#include "BufferManager.h"
#include "TransactionHandle.h"

namespace credb
{
namespace trusted
{

class TransactionBlock: public Block<TransactionHandle>, public RWLockable
{
public:
    using Block::Block;
};

    static constexpr ledger_pos_t INVALID_LEDGER_POS = {0,0};

/**
 * A registry of all committed transactions and their dependencies
 */
class TransactionLedger
{
public:
    TransactionLedger(BufferManager &buffer_manager);

    ledger_pos_t insert(const std::unordered_map<taskid_t, OpContext> &op_contexts, identity_uid_t tx_root, transaction_id_t tx_id, const op_set_t &local_ops, const std::unordered_map<identity_uid_t, op_set_t> &remote_ops);

    TransactionHandle get(ledger_pos_t pos);

private:
    PageHandle<TransactionBlock> get_pending_block(LockType lock_type)
    {
        while(true)
        {
            auto block = get_block(m_pending_block_id);

            block->lock(lock_type);

            if(block->identifier() == m_pending_block_id
                && block->is_pending())
            {
                return block;
            }
            else
            {
                block->unlock(lock_type);
            }
        }
    }

    PageHandle<TransactionBlock> get_block(block_id_t identifier)
    {
        page_no_t page_no = identifier;
        auto hdl = m_buffer_manager.get_page<TransactionBlock>(page_no);

        //FIXME handle downstream properlya
        
        return hdl;
    }

    std::mutex m_mutex;

    BufferManager &m_buffer_manager;

    /**
     * check if we should generate a new pending blokc
     *
     * note: you must hold a write lock to the pending block
     */
    void organize_ledger();

    void generate_block();

    page_no_t m_pending_block_id;
    block_index_t m_num_pending_events;

    /// Always hold a reference so the pending block is not unloaded
    PageHandle<TransactionBlock> m_pending_block;

};

inline void TransactionLedger::generate_block()
{
    m_pending_block = m_buffer_manager.new_page<TransactionBlock>(true);
    m_pending_block_id = m_pending_block->identifier();
}

}
}
