#pragma once

#include "BufferManager.h"
#include "RWLockable.h"
#include "util/defines.h"
#include <cstdint>
#include <bitstream.h>
#include <tuple>
#include <unordered_set>

namespace credb
{
namespace trusted
{

class MultiMap
{
public:
    static constexpr size_t NUM_BUCKETS = 8192;
    static constexpr size_t MAX_NODE_SIZE = 1024;

    using KeyType = int64_t;
    using ValueType = std::string;
    using bucketid_t = uint32_t;

    class node_t : public RWLockable, public Page
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

        bool remove(const KeyType &key, const ValueType &value, BufferManager &buffer);
        void find_union(const KeyType &key, std::unordered_set<ValueType> &out, BufferManager &buffer);
        void find_intersect(const KeyType &key, std::unordered_set<ValueType> &out, BufferManager &buffer);
        void find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op, BufferManager &buffer);
        size_t estimate_value_count(const KeyType &key, BufferManager &buffer);
        bool insert(const KeyType &key, const ValueType &value, BufferManager &buffer);

        /**
         *  Get the number of elements in this node
         *  @note this will return the number excluding successor
         */
        size_t size() const;

        bool empty() const
        {
            return size() == 0;
        }

        /**
         * Get entry at pos
         *
         * @param pos
         *      The position to query. Must be smaller than size()
         */
        std::pair<KeyType, ValueType> get(size_t pos) const;

        PageHandle<node_t> get_successor(LockType lock_type, bool create, BufferManager &buffer);

    private:
        bool has_entry(const KeyType &key, const ValueType &value, BufferManager &buffer);
        
        struct header_t
        {
            page_no_t successor;
            size_t size;
        };

        bitstream m_data;
    };

    class iterator_t
    {
    public:
        iterator_t(MultiMap &map, bucketid_t bpos = 0);
        iterator_t(const iterator_t &other) = delete;
        ~iterator_t();

        void clear();
        bool at_end() const;
        void operator++();
        KeyType   key() const;
        ValueType value() const;
        bool operator!=(const iterator_t &other) const;
        bool operator==(const iterator_t &other) const;

    private:
        friend class MultiMap;

        void next_bucket();

        MultiMap &m_map;
        bucketid_t m_bucket;

        std::vector<PageHandle<node_t>> m_current_nodes;
        uint32_t m_pos;
    };

    MultiMap(BufferManager &buffer, const std::string &name);
    ~MultiMap();

    void find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op);
    size_t estimate_value_count(const KeyType &key);
    bool remove(const KeyType &key, const ValueType &value);
    iterator_t begin();
    iterator_t end();
    void insert(const KeyType &key, const ValueType &value);
    void clear();
    PageHandle<node_t> get_node(const bucketid_t bucket, LockType lock_type);

    /**
     * The number of entries stored in the map
     */
    size_t size() const;

    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

private:
    friend class iterator_t;

    bucketid_t to_bucket(const KeyType key) const;

    BufferManager &m_buffer;
    const std::string m_name;
    credb::Mutex m_node_mutex;
    std::atomic<size_t> m_size;
    page_no_t m_buckets[NUM_BUCKETS];
};


} // namespace trusted
} // namespace credb
