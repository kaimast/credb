#pragma once

#include "BufferManager.h"
#include "RWLockable.h"
#include "MultiMapNode.h"
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
    static constexpr size_t NUM_SHARDS = 64;

    using KeyType = int64_t;
    using ValueType = std::string;
    using bucketid_t = uint32_t;

    using node_t = MultiMapNode<KeyType, ValueType>;

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
        void next_node();

        MultiMap &m_map;

        uint16_t m_shard_id;
        RWHandle m_shard_lock;

        bucketid_t m_bucket;

        std::vector<PageHandle<node_t>> m_current_nodes;
        uint32_t m_pos;
    };

    MultiMap(BufferManager &buffer);
    ~MultiMap();

    void find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op);

    size_t estimate_value_count(const KeyType &key);
    bool remove(const KeyType &key, const ValueType &value);
    iterator_t begin();
    iterator_t end();
    void insert(const KeyType &key, const ValueType &value);
    void clear();

    /**
     * The number of entries stored in the map
     */
    size_t size() const;

    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

private:
    void find_intersect(const KeyType &key, std::unordered_set<ValueType> &out);
    void find_union(const KeyType &key, std::unordered_set<ValueType> &out);

    PageHandle<node_t> get_node(const bucketid_t bucket_id, bool create, RWHandle &shard_lock, bool modify = false);

    PageHandle<node_t> get_successor(PageHandle<node_t>& prev, bool create, RWHandle &shard_lock, bool modify = false);

    PageHandle<node_t> get_node_internal(const page_no_t page_no, bool create, version_number expected_version, RWHandle &shard_lock);
 
    friend class iterator_t;

    RWLockable& get_shard(bucketid_t bucket_id)
    {
        return m_shards[bucket_id % NUM_SHARDS];
    }

    bucketid_t to_bucket(const KeyType key) const;

    struct bucket_t
    {
        page_no_t page_no;
        version_number version;
    };

    BufferManager &m_buffer;
    
    std::condition_variable_any m_bucket_cond;
    
    std::atomic<size_t> m_size;
    bucket_t m_buckets[NUM_BUCKETS];
    RWLockable m_shards[NUM_SHARDS];
};

} // namespace trusted
} // namespace credb
