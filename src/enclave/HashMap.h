#pragma once

#include <cstdint>
#include <bitstream.h>
#include <tuple>
#include <array>
#include <unordered_set>

#include "ObjectListIterator.h"
#include "HashMapNode.h"
#include "AbstractMap.h"

namespace credb
{
namespace trusted
{

class HashMap: public AbstractMap<HashMapNode<std::string, event_id_t>, std::string>
{
public:
    using KeyType = std::string;
    using ValueType = event_id_t; //NOTE value type must be constant size to allow updates

    using bucketid_t = AbstractMap::bucketid_t;
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

        bool get_next_key(KeyType &key) override;
        size_t count_rest() override;

    private:
        HashMap::iterator_t m_iterator;
    };

    HashMap(BufferManager &buffer, const std::string &name);
    ~HashMap();

    iterator_t begin();
    iterator_t end();

    void insert(const KeyType &key, const ValueType &value, bitstream *out_changes = nullptr);
    
    bool get(const KeyType& key, ValueType &value_out);

private:
    friend class iterator_t;
};


} // namespace trusted
} // namespace credb
