#pragma once

#include "BufferManager.h"
#include "RWLockable.h"
#include "MultiMapNode.h"
#include "util/defines.h"
#include <cstdint>
#include <bitstream.h>
#include <tuple>
#include <unordered_set>

#include "AbstractMap.h"

namespace credb
{
namespace trusted
{

class MultiMap : public AbstractMap<MultiMapNode<int64_t, std::string>, int64_t>
{
public:
    using KeyType = int64_t;
    using ValueType = std::string;
    using bucketid_t = AbstractMap::bucketid_t;

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

    MultiMap(BufferManager &buffer, const std::string &name);
    ~MultiMap();

    void find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op);

    size_t estimate_value_count(const KeyType &key);
    bool remove(const KeyType &key, const ValueType &value);
    iterator_t begin();
    iterator_t end();
    void insert(const KeyType &key, const ValueType &value);
    void clear();

private:
    void find_intersect(const KeyType &key, std::unordered_set<ValueType> &out);
    void find_union(const KeyType &key, std::unordered_set<ValueType> &out);

    friend class iterator_t;
};

} // namespace trusted
} // namespace credb
