#pragma once

#include <cstdint>
#include <bitstream.h>
#include <tuple>
#include <unordered_set>

#include "BufferManager.h"
#include "RWLockable.h"
#include "credb/event_id.h"
#include "util/defines.h"
#include "ObjectListIterator.h"

namespace credb
{
namespace trusted
{

class HashMap
{
public:
    static constexpr size_t NUM_BUCKETS = 8192;
    static constexpr size_t NUM_SHARDS = 64;
    static constexpr size_t MAX_NODE_SIZE = 1024;

    using KeyType = std::string;
    using ValueType = event_id_t; //NOTE value type must be constant size to allow updates
    using bucketid_t = uint16_t;
    using version_no_t = uint16_t;

    class node_t : public Page
    {
    public:
        // about the byte size of a bucket:
        //   (1) sizeof(*this)
        //   (2) get_unordered_container_value_byte_size = hash bucket count * sizeof
        //   std::pair<KeyType, std::unordered_set<ValueType>> (3) get_heap_size_of(KeyType) =
        //   get_heap_size_of(uint64_t) = 0
        //    *  now consider the heap size of std::unordered_set
        //    *  sizeof std::unordered_set has been counted in (2)
        //   (4) get_unordered_container_value_byte_size = hash bucket count * sizeof ValueType
        //   (5) get_heap_size_of(ValueType) = get_heap_size_of(std::string) = std::string.size()
        node_t(BufferManager &buffer, page_no_t page_no);
        node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream);

        bitstream serialize() const override;
        size_t byte_size() const override;

        bool get(const KeyType& key, ValueType &value_out);

        /**
         * Insert or update an entry
         *
         * @return True if successfully insert, False if not enough space
         */
        bool insert(const KeyType &key, const ValueType &value);

        version_no_t version_no() const;

        /**
         * Increment the version no and successor version in the header
         * Only needed for iterator
         */
        void increment_version_no();

        /**
         *  Get the number of elements in this node
         *  @note this will return the number excluding successor
         */
        size_t size() const;

        /**
         * Get entry at pos
         *
         * @param pos
         *      The position to query. Must be smaller than size()
         */
        std::pair<KeyType, ValueType> get(size_t pos) const;

        PageHandle<node_t> get_successor(bool create, BufferManager &buffer);

        /**
         * Does this node hold a specified entry?
         */
        bool has_entry(const KeyType &key, const ValueType &value);
        
    private:
        struct header_t
        {
            version_no_t version;
            page_no_t successor;
            version_no_t successor_version;
            size_t size;
        };

        bitstream m_data;
    };

    class iterator_t
    {
    public:
        iterator_t(HashMap &map, bucketid_t bpos);
        iterator_t(const iterator_t &other) = delete;
        ~iterator_t();

        iterator_t duplicate();

        void clear();
        bool at_end() const;
        void operator++();
        KeyType   key() const;
        ValueType value() const;
        bool operator!=(const iterator_t &other) const;
        bool operator==(const iterator_t &other) const;

        void set_value(const ValueType &new_value);

    private:
        iterator_t(HashMap &map, bucketid_t bpos, std::vector<PageHandle<node_t>> &current_nodes, uint32_t pos);

        friend class HashMap;

        void next_bucket();

        HashMap &m_map;

        uint16_t m_shard;
        bucketid_t m_bucket;

        std::vector<PageHandle<node_t>> m_current_nodes;
        uint32_t m_pos;
    };

    class LinearScanKeyProvider : public ObjectKeyProvider
    {
    public:
        LinearScanKeyProvider(HashMap &index);
        LinearScanKeyProvider(const ObjectListIterator &other) = delete;
        LinearScanKeyProvider(ObjectListIterator &&other) = delete;

        bool get_next_key(std::string &identifier) override;
        size_t count_rest() override;

    private:
        HashMap::iterator_t m_iterator;
    };

    HashMap(BufferManager &buffer, const std::string &name);
    ~HashMap();

    iterator_t begin();
    iterator_t end();
    void insert(const KeyType &key, const ValueType &value);
    
    bool get(const KeyType& key, ValueType &value_out);

    PageHandle<node_t> get_node(const bucketid_t bid, bool create);

    /**
     * The number of entries stored in the map
     */
    size_t size() const;

private:
    friend class iterator_t;

    bucketid_t to_bucket(const KeyType key) const;

    BufferManager &m_buffer;
    const std::string m_name;
    credb::Mutex m_node_mutex;
    std::atomic<size_t> m_size;

    struct bucket_t
    {
        page_no_t page_no;
        version_number_t version;
    };
    
    bucket_t m_buckets[NUM_BUCKETS];
    RWLockable m_shards[NUM_SHARDS];
};


} // namespace trusted
} // namespace credb
