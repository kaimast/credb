#pragma once

#include "LockHandle.h"
#include "ObjectEventHandle.h"
#include "OpContext.h"
#include "credb/defines.h"

namespace credb
{
namespace trusted
{

/**
 * ObjectIterator serves as both an object handle, as well as a tool to iterate over an object's  history
 */
class ObjectIterator
{
public:
    ~ObjectIterator();

    ObjectIterator(const OpContext &context,
                   const std::string &collection,
                   const std::string &key,
                   const std::string &path,
                   Ledger &ledger,
                   LockHandle *parent_lock_handle);

    ObjectIterator(const ObjectIterator &other) = delete;

    ObjectIterator(ObjectIterator &&other) noexcept;

    /// Drop all object references
    void clear();

    std::pair<event_id_t, json::Document> next();

    event_id_t next_handle(ObjectEventHandle &ret);


    const std::string &key() const { return m_key; }

private:
    friend class Ledger;

    bool check_event(ObjectEventHandle &event);

    const OpContext &m_context;

    const std::string m_collection;
    const std::string m_key;
    const std::string m_path;

    ObjectEventHandle m_start;
    ObjectEventHandle m_current_event;

    event_id_t m_current_eid;

    Ledger &m_ledger;
    LockHandle m_lock_handle;
};

}
}
