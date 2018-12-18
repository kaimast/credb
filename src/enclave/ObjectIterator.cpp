/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "ObjectIterator.h"
#include "Ledger.h"
#include "BufferManager.h"

namespace credb
{
namespace trusted
{

ObjectIterator::~ObjectIterator() { clear(); }

ObjectIterator::ObjectIterator(const OpContext &context,
                               const std::string &collection,
                               const std::string &key,
                               const std::string &path,
                               Ledger &ledger,
                               LockHandle *parent_lock_handle)
: m_context(context), m_collection(collection), m_key(key), m_path(path), m_ledger(ledger), m_lock_handle(ledger, parent_lock_handle)
{
    m_start = ledger.get_latest_version(m_context, m_collection, m_key, m_path, m_current_eid, m_lock_handle, LockType::Read);
}

ObjectIterator::ObjectIterator(ObjectIterator &&other) noexcept
: m_context(other.m_context), m_start(std::move(other.m_start)),
  m_current_event(std::move(other.m_current_event)), m_current_eid(other.m_current_eid),
  m_ledger(other.m_ledger), m_lock_handle(std::move(other.m_lock_handle))
{
}

bool ObjectIterator::check_event(ObjectEventHandle &event)
{
    if(!event.valid())
    {
        return false;
    }

    if(event.get_type() == ObjectEventType::Deletion)
    {
        return false;
    }

    //FIXME not sure how to check the collection policy here without causing a deadlock

    json::Document policy(event.value(), "policy", false);
    bool policy_ok = true;

    if(!policy.empty())
    {
        policy_ok = m_ledger.check_object_policy(policy, m_context, m_collection, m_key, m_path, OperationType::GetObject, m_lock_handle);
    }

    return policy_ok;
}

std::pair<event_id_t, json::Document> ObjectIterator::next()
{
    ObjectEventHandle hdl;

    auto eid = next_handle(hdl);

    if(!hdl.valid())
    {
        return {eid, json::Document()};
    }

    if(m_path.empty())
    {
        return {eid, hdl.value().duplicate()};
    }
    else
    {
        auto val = hdl.value(m_path).duplicate();

        if(val.valid())
        {
            return {eid, std::move(val)};
        }
        else
        {
            return {INVALID_EVENT, json::Document()};
        }
    }
}

event_id_t ObjectIterator::next_handle(ObjectEventHandle &ret)
{
    auto shard_no = m_ledger.get_shard(m_collection, m_key);
    ObjectEventHandle next_event;

    if(!m_current_event.valid())
    {
        m_current_event = m_start.duplicate();

        if(check_event(m_current_event))
        {
            ret = m_current_event.duplicate();
            return m_current_eid;
        }
        else
        {
            m_lock_handle.release_block(shard_no, m_current_eid.block, LockType::Read);
            m_current_eid = INVALID_EVENT;
            return INVALID_EVENT;
        }
    }

    if(!m_current_event.has_predecessor())
    {
        return INVALID_EVENT;
    }

    while(!next_event.valid() || next_event.get_type() != ObjectEventType::NewVersion)
    {
        if(!next_event.valid())
        {
            next_event = m_current_event.duplicate();
        }

        if(next_event.valid() && next_event.has_predecessor())
        {
            auto index = next_event.previous_index();
            auto pblk = next_event.previous_block();

            auto block = m_lock_handle.get_block(shard_no, pblk, LockType::Read);
            next_event = block->get(index);

            if(pblk != m_current_eid.block)
            {
                m_current_eid = { shard_no, pblk, index };
            }

            if(!check_event(next_event))
            {
                m_lock_handle.release_block(shard_no, m_current_eid.block, LockType::Read);
                m_current_eid = INVALID_EVENT;
                return INVALID_EVENT;
            }
        }
        else if(next_event.valid())
        {
            next_event.clear();
            break;
        }
        else
        {
            return INVALID_EVENT;
        }
    }

    m_current_event = std::move(next_event);
    ret = m_current_event.duplicate();
    return m_current_eid;
}

void ObjectIterator::clear()
{
    m_lock_handle.clear();
    m_current_event.clear();
}

}
}
