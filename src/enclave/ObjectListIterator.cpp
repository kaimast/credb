/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "ObjectListIterator.h"
#include "Ledger.h"
#include "logging.h"

namespace credb::trusted
{

ObjectListIterator::ObjectListIterator(const OpContext &op_context,
                                       std::string collection,
                                       json::Document predicates,
                                       Ledger &ledger,
                                       LockHandle *parent_lock_handle,
                                       std::unique_ptr<ObjectKeyProvider> keys)
: m_context(op_context), m_collection(std::move(collection)), m_predicates(std::move(predicates)),
  m_ledger(ledger), m_lock_handle(ledger, parent_lock_handle), m_keys(std::move(keys)),
  m_current_block(INVALID_BLOCK), m_current_shard(-1)
{
}

ObjectListIterator::ObjectListIterator(ObjectListIterator &&other) noexcept
: m_context(other.m_context), m_collection(other.m_collection),
  m_predicates(std::move(other.m_predicates)), m_ledger(other.m_ledger),
  m_lock_handle(std::move(other.m_lock_handle)), m_keys(std::move(other.m_keys)),
  m_current_block(other.m_current_block), m_current_shard(other.m_current_shard)
{
}

event_id_t ObjectListIterator::next(std::string &key, ObjectEventHandle &res)
{
    res.clear();
    event_id_t eid = INVALID_EVENT;

    int cnt = 0;
    bool found = false;

    while(!found && m_keys && m_keys->get_next_key(key))
    {
        auto new_shard = m_ledger.get_shard(m_collection, key);
        m_current_block = INVALID_BLOCK;

        if(new_shard != m_current_shard)
        {
            m_lock_handle.release_block(m_current_shard, m_current_block, LockType::Read);
            m_current_shard = new_shard;
        
            res = m_ledger.get_latest_version(m_context, m_collection, key, "", eid, m_lock_handle, LockType::Read);
        }
        else
        {
            // make sure we don't release the shard 
            res = m_ledger.get_latest_version(m_context, m_collection, key, "", eid, m_lock_handle, LockType::Read);
             m_lock_handle.release_block(m_current_shard, m_current_block, LockType::Read);
 
        }

        if(!res.valid())
        {
            continue;
        }

        m_current_block = eid.block;
        auto view = res.value();

        if(!view.matches_predicates(m_predicates))
        {
            ++cnt;
            res.clear();
        }
        else
        {
            found = true;
        }
    }

    if(cnt > 100)
    {
        log_debug(std::to_string(cnt) + " keys have been filterted out by the predicate");
    }

    if(found)
    {
        return eid;
    }
    else
    {
        // At end
        m_lock_handle.release_block(m_current_shard, m_current_block, LockType::Read);
        m_current_block = INVALID_BLOCK;
        return INVALID_EVENT;
    }
}



} // namespace credb::trusted
