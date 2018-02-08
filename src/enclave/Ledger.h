#pragma once

#include <cassert>
#include <cstdint>
#include <list>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "Collection.h"
#include "Object.h"
#include "OpContext.h"
#include "credb/Witness.h"
#include "util/OperationType.h"
#include "util/event_id_hash.h"

namespace credb
{
namespace trusted
{

class Shard;
class Index;

constexpr shard_id_t NUM_SHARDS = 20;

/// smaller blocks are easier to copy in and out of the enclave
constexpr size_t MIN_BLOCK_SIZE = 5 * 1024; // 5kB

class LockHandle;

class Enclave;
class Peers;
class BufferManager;

/**
 * Internal interface for the Key-value store
 * Can be tested in isolation
 */
class Ledger
{
public:
    explicit Ledger(Enclave &enclave);
    ~Ledger();

    Ledger(const Ledger &other) = delete;

    bool has_object(const std::string &collection, const std::string &key);

    event_id_t add(const OpContext &op_context,
                   const std::string &collection,
                   const std::string &key,
                   json::Document &to_add,
                   const std::string &path = "",
                   LockHandle *lock_handle_ = nullptr);

    /**
     * Create an object without a pre-specified key
     * A new, unique, key will be stored in key_out
     */
    event_id_t put_without_key(const OpContext &op_context,
                   const std::string &collection,
                   std::string &key_out,
                   json::Document &to_put,
                   LockHandle *lock_handle_ = nullptr);

    /**
     * Insert a new object or update (if an object with the specified key already exists)
     */
    event_id_t put(const OpContext &op_context,
                   const std::string &collection,
                   const std::string &key,
                   json::Document &to_put,
                   const std::string &path = "",
                   LockHandle *lock_handle_ = nullptr);


    /**
     * Mark an object as deleted
     */
    event_id_t remove(const OpContext &op_context,
                      const std::string &collection,
                      const std::string &key,
                      LockHandle *lock_handle_ = nullptr);

    /**
     * Check whether a predicate holds for a specific object
     */
    bool check(const OpContext &op_context,
               const std::string &collection,
               const std::string &key,
               const std::string &path,
               const json::Document &predicate,
               LockHandle *lock_handle_ = nullptr);

    /**
     * Count how many times a specific entity has modified an object
     */
    uint32_t count_writes(const OpContext &op_context,
                          const std::string &principal,
                          const std::string &collection,
                          const std::string &key,
                          LockHandle *lock_handle_ = nullptr);

    /**
     * @brief iterate over all versions of an object
     * @param id
     * @return
     */
    ObjectIterator iterate(const OpContext &op_context,
                           const std::string &collection,
                           const std::string &key,
                           LockHandle *lock_handle = nullptr);

    uint32_t count_objects(const OpContext &op_context,
                           const std::string &collection,
                           const json::Document &predicates = json::Document(""));

    ObjectListIterator find(const OpContext &op_context,
                            const std::string &collection,
                            const json::Document &predicates = json::Document(""),
                            LockHandle *lock_handle = nullptr);

    bool set_trigger(const std::string &collection, remote_party_id identifier);
    bool unset_trigger(const std::string &collection, remote_party_id identifier);
    void remove_triggers_for(remote_party_id identifier);

    /**
     * Returns the total number of objects in the system
     */
    uint32_t num_objects()
    {
        // TODO locking
        return m_object_count;
    }

    /**
     * Prepare a function call
     */
    bitstream prepare_call(const OpContext &op_context,
                           const std::string &collection,
                           const std::string &key,
                           const std::string &path = "");

    bool diff(const OpContext &op_context,
              const std::string &collection,
              const std::string &key,
              json::Diffs &out,
              version_number_t version1,
              version_number_t version2);

    bool create_index(const std::string &collection, const std::string &name, const std::vector<std::string> &paths);
    bool drop_index(const std::string &collection, const std::string &name);

    bool clear(const OpContext &op_context, const std::string &collection);

    bool create_witness(Witness &witness, const std::vector<event_id_t> &events);

    static bool is_valid_key(const std::string &str);

    bool get_latest_version(ObjectEventHandle &event,
                            const OpContext &op_context,
                            const std::string &collection,
                            const std::string &key,
                            const std::string &path,
                            event_id_t &id,
                            LockHandle &lock_handle,
                            LockType lock_type,
                            OperationType access_type = OperationType::GetObject);

    shard_id_t get_shard(const std::string &collection, const std::string &key)
    {
        return static_cast<shard_id_t>(hash(collection + "/" + key)) % NUM_SHARDS;
    }

    const std::unordered_map<std::string, Collection> &collections() const { return m_collections; }

    void clear_cached_blocks();
    void load_upstream_index_root(const std::vector<std::string> &collection_names);
    void put_object_index_from_upstream(size_t input_id, bitstream *input_changes, shard_id_t input_shard, page_no_t input_block_page_no);

    void unload_everything(); // for debug purpose
    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

    // Needed by object iterators
    // TODO move out of ledger class?
    bool check_object_policy(const json::Document &policy,
                             const OpContext &op_context,
                             const std::string &collection,
                             const std::string &key,
                             const std::string &path,
                             OperationType type,
                             LockHandle &lock_handle);

    /**
     * check the collection's policy (if it exists)
     *
     * @note make sure to call this before holding any locks to the object
     */
    bool check_collection_policy(const OpContext &op_context,
                                 const std::string &collection,
                                 const std::string &key,
                                 const std::string &path,
                                 OperationType type);

private:
    Enclave &m_enclave;
    BufferManager &m_buffer_manager;

    friend class Transaction;
    friend class LockHandle;
    friend class ObjectListIterator;

    bool get_event(ObjectEventHandle &out, const event_id_t &eid, LockHandle &lock_handle, LockType lock_type);
    bool get_previous_event(shard_id_t shard_no,
                            ObjectEventHandle &previous,
                            const ObjectEventHandle &current,
                            LockHandle &lock_handle,
                            LockType lock_type);

    /// This function takes in a an event and finds a version of the object that is <= event
    /// It might need to lock a new Block which will be stored in previous_version_block
    bool get_previous_version(shard_id_t shard_no,
                              ObjectEventHandle &out,
                              const ObjectEventHandle &current_event,
                              LockHandle &lock_handle,
                              LockType lock_type);

    bool get_latest_event(ObjectEventHandle &hdl,
                          const std::string &collection,
                          const std::string &key,
                          event_id_t &event_id,
                          LockHandle &lock_handle,
                          LockType lock_type);

    event_index_t put_tombstone(const OpContext &op_context,
                                const std::string &collection,
                                const std::string &key,
                                const event_id_t &previous_id,
                                LockHandle &lock_handle);

    event_id_t put_next_version(const OpContext &op_context,
                                const std::string &collection,
                                const std::string &key,
                                json::Document &doc,
                                version_number_t version_number,
                                event_id_t previous_id,
                                const ObjectEventHandle &previous_version,
                                LockHandle &lock_handle);

    Collection *try_get_collection(const std::string &name)
    {
        auto it = m_collections.find(name);

        if(it == m_collections.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    Collection &get_collection(const std::string &name, bool create = false)
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

    void organize_ledger(uint16_t shard_no);

    void send_index_updates_to_downstream(const bitstream &index_changes, shard_id_t shard, page_no_t invalidated_page);

    Shard *m_shards[NUM_SHARDS];

    std::unordered_map<std::string, Collection> m_collections;

    uint32_t m_object_count;
    uint64_t m_version_count;

    credb::Mutex m_put_object_index_mutex;
    size_t m_put_object_index_last_id;
    bool m_put_object_index_running;
    using put_object_index_item_t = std::tuple<size_t, bitstream *, shard_id_t, page_no_t>;
    struct put_object_index_item_t_compare_t
    {
        bool operator()(const put_object_index_item_t &t1, const put_object_index_item_t &t2)
        {
            return std::get<0>(t1) > std::get<0>(t2);
        }
    };
    std::priority_queue<put_object_index_item_t, std::vector<put_object_index_item_t>, put_object_index_item_t_compare_t> m_put_object_index_queue;
};

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

} // namespace trusted
} // namespace credb
