#pragma once

#include <cstdint>
#include <bitstream.h>
#include <tuple>
#include <array>
#include <unordered_set>

#include "version_number.h"
#include "BufferManager.h"
#include "RWLockable.h"
#include "credb/event_id.h"
#include "util/defines.h"
#include "ObjectListIterator.h"
#include "HashMapNode.h"

namespace credb
{
namespace trusted
{

class HashMap
{
public:
    static constexpr size_t NUM_BUCKETS = 8192;
    static constexpr size_t NUM_SHARDS = 64;

    using KeyType = std::string;
    using ValueType = event_id_t; //NOTE value type must be constant size to allow updates
    using bucketid_t = uint16_t;

    using node_t = HashMapNode<KeyType, ValueType>;

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

        void set_value(const ValueType &new_value, bitstream *out_changes = nullptr);

    private:
        iterator_t(HashMap &map, bucketid_t bpos, std::vector<PageHandle<node_t>> &current_nodes, uint32_t pos);

        friend class HashMap;

        void next_bucket();

        HashMap &m_map;

        uint16_t m_shard_id;
        RWHandle m_shard_lock;

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

    void apply_changes(bitstream &changes);

    iterator_t begin();
    iterator_t end();

    void insert(const KeyType &key, const ValueType &value, bitstream *out_changes = nullptr);
    
    bool get(const KeyType& key, ValueType &value_out);

    /**
     * The number of entries stored in the map
     */
    size_t size() const;

    void serialize_root(bitstream &out);

    void load_root(bitstream &in);

private:
    friend class iterator_t;

    PageHandle<node_t> get_node(const bucketid_t bid, bool create, RWHandle &shard_lock);

    PageHandle<node_t> get_successor(PageHandle<node_t>& prev, bool create, RWHandle &shard_lock);

    PageHandle<node_t> get_node_internal(const page_no_t page_no, bool create, version_number expected_version, RWHandle &shard_lock);
    
    bucketid_t to_bucket(const KeyType key) const;

    BufferManager &m_buffer;
    const std::string m_name;

    struct bucket_t
    {
        page_no_t page_no;
        version_number version;
    };
    
    std::condition_variable_any m_bucket_cond;
    
    std::atomic<size_t> m_size;
    bucket_t m_buckets[NUM_BUCKETS];

    std::array<RWLockable, NUM_SHARDS> m_shards;
};


} // namespace trusted
} // namespace credb
