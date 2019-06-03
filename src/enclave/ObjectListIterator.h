/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <memory>

#include "LockHandle.h"
#include "ObjectEventHandle.h"
#include "OpContext.h"
#include "ObjectIterator.h"
#include "ObjectKeyProvider.h"

namespace credb::trusted
{

class ObjectListIterator
{
public:
    ObjectListIterator(const OpContext &op_context,
                       std::string collection,
                       json::Document predicates,
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

    // Shouldn't be modified except by move constructor
    json::Document m_predicates;

    Ledger &m_ledger;
    LockHandle m_lock_handle;
    std::unique_ptr<ObjectKeyProvider> m_keys;

    block_id_t m_current_block;
    shard_id_t m_current_shard;
};

} // namespace credb::trusted

