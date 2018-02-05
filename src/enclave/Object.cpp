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


} // namespace trusted
} // namespace credb
