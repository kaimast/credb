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

// TODO: SIGILL happens if HeapMaxSize=96MB and BufferManager=70MB
//       i guess it's because some uncounted memory of std::unordered_{set,map}
//       maybe we need to write our own std::unordered_{set,map}
//       we have this issue both before and after using the new paging mechanism
class MultiMap
{
public:
    static constexpr size_t NUM_BUCKETS = 8192;
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
        using bucket_t = std::unordered_map<KeyType, std::unordered_set<ValueType>>;

        node_t(BufferManager &buffer, page_no_t page_no);
        node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream);

        bitstream serialize() const override;
        size_t byte_size() const override;

        bool remove(const KeyType &key, const ValueType &value);
        void find_union(const KeyType &key, std::unordered_set<ValueType> &out);
        void find_intersect(const KeyType &key, std::unordered_set<ValueType> &out);
        void find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op);
        size_t estimate_value_count(const KeyType &key);
        bool insert(const KeyType &key, const ValueType &value);
        const bucket_t &get_bucket() const { return m_bucket; }

    private:
        bucket_t m_bucket;
        size_t m_byte_size;
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
        const KeyType &key() const;
        const ValueType &value() const;
        bool operator!=(const iterator_t &other) const;
        bool operator==(const iterator_t &other) const;

    private:
        friend class MultiMap;

        void next_bucket();
        auto tie() const { return std::tie(m_bucket, m_map_iter, m_set_iter); }

        MultiMap &m_map;
        bucketid_t m_bucket;

        PageHandle<node_t> m_current_node;
        std::unordered_map<KeyType, std::unordered_set<ValueType>>::const_iterator m_map_iter;
        std::unordered_set<ValueType>::const_iterator m_set_iter;
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
    size_t size() const;
    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

private:
    friend class iterator_t;
    bucketid_t to_bucket(const KeyType key) const;

    BufferManager &m_buffer;
    const std::string m_name;
    credb::Mutex m_size_mutex;
    size_t m_size;
    page_no_t m_buckets[NUM_BUCKETS];
};


} // namespace trusted
} // namespace credb
