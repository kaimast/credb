#if 0
#pragma once

#include <cstring>
#include <functional>
#include <stdint.h>
#include <vector>

#include <bitstream.h>

#include "Lockable.h"
#include "hash.h"
#include "logging.h"

#include "StaticPageable.h"

#ifndef TEST
#include "Enclave.h"
#endif

#include "StaticPageableNode.h"

template<typename T> inline
size_t get_heap_size_of(const T &t)
{
    return 0;
}

template<> inline
size_t get_heap_size_of<std::string>(const std::string &str)
{
    return str.size();
}

namespace credb
{
namespace trusted
{

/// A pageable hashmap datastructure
template<size_t NUM_BUCKETS, size_t NUM_NODES, size_t MAX_BYTE_SIZE, typename KeyType, typename ValueType>
class HashMap
{
public:
typedef uint32_t bucketid_t;

private:
    static constexpr size_t buckets_per_node()
    {
        return NUM_BUCKETS / NUM_NODES;
    }

    class node_t : public StaticPageableNode
    {
    public:
        struct bucket_entry_t
        {
            KeyType key;
            ValueType value;

            size_t byte_size() const
            {
                return sizeof(*this) + get_heap_size_of(key) + get_heap_size_of(value);
            }
        };

        typedef std::vector<bucket_entry_t> bucket_t;

        node_t()
            : StaticPageableNode(), m_buckets(), m_byte_size(0)
        {}

        node_t(const node_t &other) = delete;

        virtual ~node_t() {}

        bool empty() const
        {
            return m_byte_size == 0;
        }

        void parse(bitstream &input) override
        {
            for(uint32_t i = 0; i < buckets_per_node(); ++i)
            {
                uint32_t bsize;
                input >> bsize;

                auto &bucket = m_buckets[i];
                bucket.resize(bsize);

                for(uint32_t j = 0; j < bsize; ++j)
                {
                    bucket_entry_t e;
                    input >> e.key;
                    input >> e.value;
                    bucket[j] = e;
                    m_byte_size += e.byte_size();
                }
            }
        }

        void serialize(bitstream &output) override
        {
            for(uint32_t i = 0; i < buckets_per_node(); ++i)
            {
                auto &bucket = m_buckets[i];
                output << static_cast<uint32_t>(bucket.size());

                for(auto &it: bucket)
                    output << it.key << it.value;
            }
        }

        const bucket_t& get_bucket(const bucketid_t b) const
        {
            return m_buckets[b % buckets_per_node()];
        }

        bucket_t& get_bucket(const bucketid_t b)
        {
            return m_buckets[b % buckets_per_node()];
        }

        bool contains(const KeyType &key, const bucketid_t b)
        {
            bool res = false;

            auto &bucket = get_bucket(b);
            for(auto &e : bucket)
            {
                if(e.key == key)
                {
                    res = true;
                    break;
                }
            }

            return res;
        }

        bool insert(const bucketid_t b, const KeyType &key, const ValueType &value)
        {
            bucket_t &bucket = get_bucket(b);

            for(auto &e : bucket)
            {
                if(e.key == key)
                {
                    m_byte_size -= e.byte_size();
                    e.value = value;
                    m_byte_size += e.byte_size();
                    return false;
                }
            }

            bucket_entry_t e = {key, value};
            m_byte_size += e.byte_size();
            bucket.push_back(e);
            return true;
        }

        void unload() override
        {
            for(auto &bucket: m_buckets)
               bucket.clear();

            m_byte_size = 0;
            m_is_loaded = false;
        }

        size_t byte_size() const
        {
            return m_byte_size;
        }

        uint64_t last_used;

    private:
        bucket_t m_buckets[buckets_per_node()];

        friend class HashMap;
       size_t m_byte_size;
    };

public:
    class iterator_t
    {
    public:
        iterator_t()
            : m_map(nullptr), m_current_node(nullptr), m_bucket(NUM_BUCKETS)
        {}

        iterator_t(HashMap &map, bucketid_t bpos = 0)
            : m_map(&map), m_current_node(nullptr), m_bucket(bpos), m_bit()
        {
            next_bucket();
        }

        iterator_t(HashMap &map, const KeyType &key, node_t *node, bucketid_t bpos)
            : m_map(&map), m_current_node(node), m_bucket(bpos), m_bit()
        {
            move_to(key);
        }

        iterator_t(const iterator_t &other) = delete;

        ~iterator_t()
        {
            clear();
        }

        bool at_end() const
        {
            return m_bucket >= NUM_BUCKETS;
        }

        void move(iterator_t &other)
        {
            m_current_node = other.m_current_node;
            m_bit = other.m_bit;
            m_bucket = other.m_bucket;
            m_map = other.m_map;

            other.m_current_node = nullptr;
            other.m_bucket = NUM_BUCKETS;
        }

        /// Drops all locks and invalidates iterator
        void clear()
        {
            if(!m_current_node)
                return;

            m_current_node->read_unlock();
            m_map->page_manager.check_evict();
            m_current_node = nullptr;
            m_bucket = NUM_BUCKETS;
       }

        // FIXME SGX misses support for move semantics
        //iterator_t(const iterator_t &other) = delete;
        void operator++()
        {
            if(m_bucket == NUM_BUCKETS)
                throw std::runtime_error("Alreayd at end");

            m_bit++;

            if(m_bit == m_current_node->get_bucket(m_bucket).end())
            {
                m_bucket++;
                next_bucket();
            }
        }

        const typename HashMap::node_t::bucket_entry_t& operator*() const
        {
            return *m_bit;
        }

        const typename HashMap::node_t::bucket_entry_t* operator->() const
        {
            return &(*m_bit);
        }

        void set(const ValueType &val)
        {
            m_bit->value = val;
        }

        bool operator!=(const iterator_t &other) const
        {
            return !(*this == other);
        }

        bool operator==(const iterator_t &other) const
        {
            if(m_bucket == NUM_BUCKETS && other.m_bucket == NUM_BUCKETS)
                return true;

            if(m_bucket == NUM_BUCKETS || other.m_bucket == NUM_BUCKETS)
                return false;

            assert(m_current_node != nullptr && other.m_current_node != nullptr);
            return (other.m_bucket == m_bucket) && (other.m_bit == m_bit);
        }

    private:
        friend class HashMap;

        void move_to(const KeyType &key)
        {
            if(m_current_node == nullptr)
                throw std::runtime_error("Cannot move without a current node");

            auto &bucket = m_current_node->get_bucket(m_bucket);
            m_bit = bucket.begin();

            while(m_bit->key != key)
                m_bit++;

            if(m_bit == bucket.end())
                throw std::runtime_error("Could not find key!");
        }

        void next_bucket()
        {
            while(!at_end())
            {
                if(m_current_node)
                {
                    m_current_node->read_unlock();
                }

                m_current_node = nullptr;
                m_current_node = m_map->get_node(m_bucket, LockType::Read);

                if(m_current_node)
                {
                    auto &bucket = m_current_node->get_bucket(m_bucket);
                    m_bit = bucket.begin();

                    if(m_bit != bucket.end())
                        break;
                }

                m_bucket++;
            }

            if(m_bucket == NUM_BUCKETS && m_current_node != nullptr)
            {
                m_current_node->read_unlock();
                m_current_node = nullptr;
            }
        }

        HashMap *m_map;
        typename HashMap::node_t *m_current_node;
        bucketid_t m_bucket;
        typename HashMap::node_t::bucket_t::iterator m_bit;
    };

    HashMap(const std::string& name) : page_manager(name), m_size(0)
    {
    }

    void find(iterator_t &it, const KeyType &key)
    {
        auto b = to_bucket(key);
        auto node = get_node(b, LockType::Read);

        if(!node->contains(key, b))
        {
            node->read_unlock();
            return;
        }

        iterator_t it_(*this, key, node, b);
        it.move(it_);
    }

    void begin(iterator_t &it)
    {
        iterator_t it_(*this, 0);
        it.move(it_);
    }

    bool erase(iterator_t &it)
    {
        it.m_current_node->read_to_write_lock();
        auto &bucket = it.m_current_node->get_bucket(it.m_bucket);

        auto offset = it->byte_size();
        it.m_current_node->m_byte_size -= offset;
        it.m_bit = bucket.erase(it.m_bit);

        it.m_current_node->write_to_read_lock();

        if(it.m_bit == bucket.end())
            it.next_bucket();

        page_manager.decrease_byte_size(offset);

        m_size_lock.lock();
        m_size -= 1;
        m_size_lock.unlock();
        return true;
    }

    void insert(const KeyType &key, const ValueType &value)
    {
        auto b = to_bucket(key);
        auto node = get_node(b, LockType::Write);

        size_t previous_size = node->byte_size();

        bool created = node->insert(b, key, value);
        if(created)
        {
            m_size_lock.lock();
            m_size += 1;
            m_size_lock.unlock();
        }

        size_t new_size = node->byte_size();
        node->write_unlock();

        if(new_size > previous_size)
            page_manager.increase_byte_size(new_size - previous_size);
        else if(previous_size > new_size)
            page_manager.decrease_byte_size(previous_size - new_size);
    }

    size_t size() const
    {
        return m_size;
    }

private:
    friend class iterator_t;

    StaticPageable<size_t, node_t, MAX_BYTE_SIZE, NUM_NODES> page_manager;

    bucketid_t to_bucket(const KeyType key) const
    {
        return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
    }

    /// Gets a node and acquires a read_lock for you
    node_t* get_node(const bucketid_t &b, LockType lock_type)
    {
        const size_t pos = floor(static_cast<float>(b) / buckets_per_node());
        return &page_manager.get_block(pos, lock_type);
    }

    size_t m_size;
    Lockable m_size_lock;
};

}
}
#endif
