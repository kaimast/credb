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
#include "ObjectListIterator.h"
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

    bool has_object(const std::string &collection, const std::string &key,
                    LockHandle *lock_handle_ = nullptr);

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
                           const std::string &path = "",
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

    void get_next_event_ids(std::unordered_set<event_id_t> &out, shard_id_t shard, uint16_t num, LockHandle *lock_handle_);

    /**
     * Returns the total number of objects in the system
     */
    size_t num_objects()
    {
        // TODO locking
        return m_object_count;
    }

    uint32_t num_collections()
    {
        return m_collections.size();
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

    ObjectEventHandle get_latest_version(const OpContext &op_context,
                            const std::string &collection,
                            const std::string &key,
                            const std::string &path,
                            event_id_t &id,
                            LockHandle &lock_handle,
                            LockType lock_type,
                            OperationType access_type = OperationType::GetObject);

    shard_id_t get_shard(const std::string &collection, const std::string &key);

    const std::unordered_map<std::string, Collection> &collections() const;

    std::unordered_map<std::string, Collection> &collections();

    void clear_cached_blocks();

    void put_object_index_from_upstream(bitstream &changes, shard_id_t shard_id, page_no_t block_page_no, Block::int_type block_size);

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
                                 OperationType type,
                                 LockHandle *lock_handle_);

    bool prepare_write(const OpContext &op_context,
                             const std::string &collection,
                             const std::string &key,
                             const std::string &path,
                             OperationType op_type,
                             LockHandle *lock_handle_);

    event_id_t apply_write(const OpContext &op_context,
                       const std::string &collection,
                       const std::string &key,
                       json::Document &to_write,
                       const std::string &path,
                       LockHandle *lock_handle_,
                       OperationType op_type,
                       const std::unordered_set<event_id_t> &read_set,
                       const std::unordered_set<event_id_t> &write_set);

    Collection &get_collection(const std::string &name, bool create = false);

private:
    Enclave &m_enclave;
    BufferManager &m_buffer_manager;

    friend class Transaction;
    friend class LockHandle;
    friend class ObjectListIterator;

    ObjectEventHandle get_event(const event_id_t &eid, LockHandle &lock_handle, LockType lock_type);

    ObjectEventHandle get_previous_event(shard_id_t shard_no,
                            const ObjectEventHandle &current,
                            LockHandle &lock_handle,
                            LockType lock_type);

    /// This function takes in an event and finds a version of the object that is <= event
    /// It might need to lock a new Block which will be stored in previous_version_block
    ObjectEventHandle get_previous_version(shard_id_t shard_no,
                              const ObjectEventHandle &current_event,
                              LockHandle &lock_handle,
                              LockType lock_type);

    ObjectEventHandle get_latest_event(const std::string &collection,
                          const std::string &key,
                          event_id_t &event_id,
                          LockHandle &lock_handle,
                          LockType lock_type);

    event_id_t put_tombstone(const OpContext &op_context,
                                const event_id_t &previous_id,
                                LockHandle &lock_handle);

    event_id_t put_next_version(const OpContext &op_context,
                                const std::string &collection,
                                const std::string &key,
                                json::Document &doc,
                                version_number_t version_number,
                                event_id_t previous_id,
                                const ObjectEventHandle &previous_version,
                                LockHandle &lock_handle,
                                const std::unordered_set<event_id_t> &read_set,
                                const std::unordered_set<event_id_t> &write_set);

    Collection *try_get_collection(const std::string &name);


    void organize_ledger(uint16_t shard_no);

    void send_index_updates_to_downstream(const bitstream &index_changes, shard_id_t shard, page_no_t invalidated_page, Block::int_type block_size);

    Shard *m_shards[NUM_SHARDS];

    std::unordered_map<std::string, Collection> m_collections;

    size_t m_object_count;
    size_t m_version_count;
};

} // namespace trusted
} // namespace credb

#include "Ledger.inl"
