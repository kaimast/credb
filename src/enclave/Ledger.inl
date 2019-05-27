#include "logging.h"
#include "HashMap.h"

namespace credb
{
namespace trusted
{

inline bool Ledger::is_valid_key(const std::string &str)
{
    if(str.empty())
        return false;

    for(auto c : str)
    {
        if(!isalnum(c) && c != '_')
            return false;
    }

    return true;
}

inline Collection& Ledger::get_collection(const std::string &name, bool create)
{
    // FIXME locking
    auto it = m_collections.find(name);

    if(it == m_collections.end())
    {
        if(!create)
            throw std::runtime_error("No such collection: " + name);

        m_collections.emplace(name, Collection(m_buffer_manager, name));
        it = m_collections.find(name);
    }

    return it->second;
}

inline bool Ledger::prepare_write(const OpContext &op_context,
                             const std::string &collection,
                             const std::string &key,
                             const std::string &path,
                             OperationType op_type,
                             LockHandle *lock_handle_)
{
    if(!Ledger::is_valid_key(key))
    {
        return false;
    }

    if(!check_collection_policy(op_context, collection, key, path, op_type, lock_handle_))
    {
        log_debug("rejected add because of collection policy");
        return false;
    }

    return true;
}

inline event_id_t Ledger::add(const OpContext &op_context,
                       const std::string &collection,
                       const std::string &key,
                       json::Document &to_add,
                       const std::string &path,
                       LockHandle *lock_handle_)
{
    const auto transaction_ref = INVALID_LEDGER_POS;

    if(prepare_write(op_context, collection, key, path, OperationType::AddToObject, lock_handle_))
    {
        return apply_write(op_context, collection, key, to_add, path, lock_handle_, OperationType::AddToObject, transaction_ref);
    }
    else
    {
        return INVALID_EVENT;
    }
}

inline event_id_t Ledger::remove(const OpContext &op_context, const std::string &collection, const std::string &key, LockHandle *lock_handle_)
{
    const auto transaction_ref = INVALID_LEDGER_POS;


    const std::set<event_id_t> read_set, write_set;
    json::Document to_write;
    const std::string path;

    if(prepare_write(op_context, collection, key, path, OperationType::AddToObject, lock_handle_))
    {
        return apply_write(op_context, collection, key, to_write, path, lock_handle_, OperationType::RemoveObject, transaction_ref);
    }
    else
    {
        return INVALID_EVENT;
    }
}

inline event_id_t Ledger::put(const OpContext &op_context,
                       const std::string &collection,
                       const std::string &key,
                       json::Document &to_put,
                       const std::string &path,
                       LockHandle *lock_handle_)
{
    const auto transaction_ref = INVALID_LEDGER_POS;

    if(prepare_write(op_context, collection, key, path, OperationType::PutObject, lock_handle_))
    {
        return apply_write(op_context, collection, key, to_put, path, lock_handle_, OperationType::PutObject, transaction_ref);
    }
    else
    {
        return INVALID_EVENT;
    }
}

inline Collection* Ledger::try_get_collection(const std::string &name)
{
    auto it = m_collections.find(name);

    if(it == m_collections.end())
    {
        return nullptr;
    }

    return &it->second;
}

inline shard_id_t Ledger::get_shard(const std::string &collection, const std::string &key)
{
    return static_cast<shard_id_t>(hash(collection + "/" + key)) % NUM_SHARDS;
}

inline const std::unordered_map<std::string, Collection>& Ledger::collections() const
{
    return m_collections;
}

inline std::unordered_map<std::string, Collection>& Ledger::collections()
{
    return m_collections;
}

inline ObjectEventHandle Ledger::get_latest_event(
                              const std::string &collection,
                              const std::string &key,
                              event_id_t &event_id,
                              LockHandle &lock_handle,
                              LockType lock_type)
{
    event_id = INVALID_EVENT;

    if(key.empty())
    {
        return ObjectEventHandle();
    }

    auto p_col = try_get_collection(collection);

    if(!p_col)
    {
        return ObjectEventHandle();
    }

    auto &col = *p_col;

    if(!col.primary_index().get(key, event_id))
    {
        return ObjectEventHandle();
    }

    auto block = lock_handle.get_block(event_id.shard, event_id.block, lock_type);
    return block->get(event_id.index);
}



}
}
