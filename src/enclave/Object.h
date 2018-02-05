#pragma once

#include <json/json.h>
#include <set>

#include "LockHandle.h"
#include "ObjectEventHandle.h"
#include "OpContext.h"
#include "ObjectIterator.h"
#include "credb/defines.h"

namespace credb
{
namespace trusted
{

class ObjectKeyProvider
{
public:
    virtual ~ObjectKeyProvider();
    virtual bool get_next_key(std::string &identifier) = 0;
    virtual size_t count_rest() = 0;
};

class ObjectListIterator
{
public:
    ObjectListIterator(const OpContext &op_context,
                       const std::string &collection,
                       const json::Document &predicates,
                       Ledger &ledger,
                       LockHandle *parent_lock_handle,
                       std::unique_ptr<ObjectKeyProvider> keys);
    ObjectListIterator(const ObjectListIterator &other) = delete;
    ObjectListIterator(ObjectListIterator &&other) noexcept;

    /**
     * Returns the next object in the list
     * The result will be INVALID_EVENT  if the list is exhausted
     * Output will be stored in key (the key of the element) and event (the object)
     */
    event_id_t next(std::string &key, ObjectEventHandle &res);

private:
    const OpContext &m_context;

    const std::string m_collection;

    json::Document m_predicates;

    Ledger &m_ledger;
    LockHandle m_lock_handle;
    std::unique_ptr<ObjectKeyProvider> m_keys;

    block_id_t m_current_block;
    shard_id_t m_current_shard;
};

// TODO: maybe provide better key providers for other indexes and remove this in the future?
class VectorObjectKeyProvider : public ObjectKeyProvider
{
public:
    VectorObjectKeyProvider(std::vector<std::string> &&identifiers) noexcept;
    VectorObjectKeyProvider(VectorObjectKeyProvider &&other) noexcept;
    virtual bool get_next_key(std::string &identifier);
    virtual size_t count_rest();

private:
    std::vector<std::string> m_identifiers;
    std::vector<std::string>::const_iterator m_iterator;
};

} // namespace trusted
} // namespace credb
