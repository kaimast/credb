#pragma once

#include <json/json.h>
#include <set>

#include "LockHandle.h"
#include "ObjectEventHandle.h"
#include "OpContext.h"
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

/// ObjectIterator serves as both an object handle, as well as a tool to iterate over an object's
/// history
class ObjectIterator
{
public:
    ~ObjectIterator();

    ObjectIterator(const OpContext &context,
                   const std::string &collection,
                   const std::string &key,
                   Ledger &ledger,
                   LockHandle *parent_lock_handle);

    ObjectIterator(const ObjectIterator &other) = delete;

    ObjectIterator(ObjectIterator &&other) noexcept;

    /// Drop all object references
    void clear();

    event_id_t next_value(json::Document &out, const std::string &path = "")
    {
        ObjectEventHandle hdl;
        auto res = next(hdl);
        if(!res)
        {
            return INVALID_EVENT;
        }

        try
        {
            out = hdl.value(path);
        }
        catch(std::runtime_error &e)
        {
            return INVALID_EVENT;
        }

        return res;
    }

    event_id_t next(ObjectEventHandle &ret);

    const std::string &key() const { return m_key; }

private:
    friend class Ledger;

    bool check_event(ObjectEventHandle &event);

    const OpContext &m_context;

    const std::string m_collection;
    const std::string m_key;

    ObjectEventHandle m_start;
    ObjectEventHandle m_current_event;

    event_id_t m_current_eid;

    Ledger &m_ledger;
    LockHandle *m_lock_handle;
};

} // namespace trusted
} // namespace credb
