#include "Object.h"
#include "Block.h"
#include "Ledger.h"
#include "LockHandle.h"
#include "logging.h"
#include "BufferManager.h"

namespace credb
{
namespace trusted
{

//------ ObjectKeyProvider

ObjectKeyProvider::~ObjectKeyProvider() = default;


//------ ObjectListIterator

ObjectListIterator::ObjectListIterator(const OpContext &op_context,
                                       const std::string &collection,
                                       const json::Document &predicates,
                                       Ledger &ledger,
                                       LockHandle *parent_lock_handle,
                                       std::unique_ptr<ObjectKeyProvider> keys)
: m_context(op_context), m_collection(collection), m_predicates(predicates.duplicate()),
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
    while(!res.valid() && m_keys && m_keys->get_next_key(key))
    {
        m_lock_handle.release_block(m_current_shard, m_current_block, LockType::Read);

        m_current_block = INVALID_BLOCK;
        m_current_shard = m_ledger.get_shard(m_collection, key);

        if(!m_ledger.get_latest_version(res, m_context, m_collection, key, "", eid, m_lock_handle, LockType::Read))
        {
            continue;
        }

        m_current_block = eid.block;
        auto view = res.value();

        if(!view.matches_predicates(m_predicates))
        {
            ++cnt;
            m_lock_handle.release_block(m_current_shard, m_current_block, LockType::Read);
            m_current_block = INVALID_BLOCK;
            res.clear();
        }
    }

    if(cnt > 100)
    {
        log_debug(std::to_string(cnt) + " keys have been filterted out by the predicate");
    }

    if(res.valid())
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


//------ VectorObjectKeyProvider

VectorObjectKeyProvider::VectorObjectKeyProvider(std::vector<std::string> &&identifiers) noexcept
: m_identifiers(std::move(identifiers)), m_iterator(m_identifiers.cbegin())
{
}

VectorObjectKeyProvider::VectorObjectKeyProvider(VectorObjectKeyProvider &&other) noexcept
: m_identifiers(other.m_identifiers), m_iterator(other.m_iterator)
{
}

bool VectorObjectKeyProvider::get_next_key(std::string &identifier)
{
    if(m_iterator == m_identifiers.end())
    {
        return false;
    }

    identifier = *m_iterator;
    ++m_iterator;
    return true;
}

size_t VectorObjectKeyProvider::count_rest()
{
    return m_identifiers.size() - (m_identifiers.cbegin() - m_iterator);
}


//------ ObjectIterator

ObjectIterator::~ObjectIterator() { clear(); }

ObjectIterator::ObjectIterator(const OpContext &context,
                               const std::string &collection,
                               const std::string &key,
                               Ledger &ledger,
                               LockHandle *parent_lock_handle)
: m_context(context), m_collection(collection), m_key(key), m_ledger(ledger)
{
    m_lock_handle = new LockHandle(ledger, parent_lock_handle);

    ledger.get_latest_version(m_start, m_context, m_collection, m_key, "", m_current_eid,
                              *m_lock_handle, LockType::Read);
}

ObjectIterator::ObjectIterator(ObjectIterator &&other) noexcept
: m_context(other.m_context), m_start(std::move(other.m_start)),
  m_current_event(std::move(other.m_current_event)), m_current_eid(other.m_current_eid),
  m_ledger(other.m_ledger), m_lock_handle(other.m_lock_handle)
{
    other.m_lock_handle = nullptr;
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

    json::Document policy(event.value(), "policy", false);
    bool policy_ok = true;

    if(!policy.empty())
    {
        // FIXME path?
        policy_ok = m_ledger.check_security_policy(policy, m_context, m_collection, m_key, "",
                                                   OperationType::GetObject, *m_lock_handle);
    }

    return policy_ok;
}

event_id_t ObjectIterator::next(ObjectEventHandle &ret)
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
            m_lock_handle->release_block(shard_no, m_current_eid.block, LockType::Read);
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

            auto block = m_lock_handle->get_block(shard_no, pblk, LockType::Read);
            block->get_event(next_event, index);

            if(pblk != m_current_eid.block)
            {
                m_current_eid = { shard_no, pblk, index };
            }

            if(!check_event(next_event))
            {
                m_lock_handle->release_block(shard_no, m_current_eid.block, LockType::Read);
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
    delete m_lock_handle;
    m_lock_handle = nullptr;
    m_current_event.clear();
}

} // namespace trusted
} // namespace credb
