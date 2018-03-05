#include "MultiMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"
#include "util/hash.h"

#include "credb/defines.h"

namespace credb
{
namespace trusted
{

MultiMap::iterator_t::iterator_t(MultiMap &map, bucketid_t bpos)
: m_map(map), m_bucket(bpos), m_pos(0)
{
    next_bucket();
    next_node();
}

MultiMap::iterator_t::~iterator_t() { clear(); }

bool MultiMap::iterator_t::at_end() const { return m_bucket >= NUM_BUCKETS; }

void MultiMap::iterator_t::clear()
{
    m_shard_lock.clear();
    m_current_nodes.clear();
    m_bucket = NUM_BUCKETS;
    m_shard_id = NUM_SHARDS;
}

void MultiMap::iterator_t::operator++()
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    if(m_current_nodes.empty())
    {
        throw std::runtime_error("Invalid state");
    }

    m_pos += 1;

    next_node();
}

void MultiMap::iterator_t::next_node()
{
    // Nodes might be empty so we got to loop here
    while(m_bucket < NUM_BUCKETS)
    {
        auto &current = *m_current_nodes.rbegin();

        if(current && current->size() > m_pos)
        {
            break;
        }

        std::vector<page_no_t> parents;

        for(auto &n : m_current_nodes)
        {
            parents.push_back(n->page_no());
        }

        auto succ = m_map.get_successor(m_bucket, current, parents, false, m_shard_lock);
        m_pos = 0;

        if(succ)
        {
            m_current_nodes.emplace_back(std::move(succ));
        }
        else
        {
            m_bucket++;
            next_bucket();
        }
    }
}

MultiMap::KeyType MultiMap::iterator_t::key() const
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }
    
    auto &cur = *m_current_nodes.rbegin();
    auto res = cur->get(m_pos);

    return std::get<0>(res);
}

MultiMap::ValueType MultiMap::iterator_t::value() const
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    auto &cur = *m_current_nodes.rbegin();
    auto res = cur->get(m_pos);

    return std::get<1>(res);
}

bool MultiMap::iterator_t::operator!=(const iterator_t &other) const
{
    return !(*this == other);
}

bool MultiMap::iterator_t::operator==(const iterator_t &other) const
{
    return m_bucket == other.m_bucket
        && m_current_nodes.size() == other.m_current_nodes.size()
        && m_pos == other.m_pos;
}

void MultiMap::iterator_t::next_bucket()
{
    while(m_bucket < NUM_BUCKETS)
    {
        m_current_nodes.clear();
        auto shard = m_bucket % MultiMap::NUM_SHARDS;
        
        if(shard != m_shard_id)
        {
            m_shard_lock.clear();

            m_shard_id = shard;
            m_shard_lock = ReadLock(m_map.get_shard(m_bucket));
        }

        auto current = m_map.get_node(m_bucket, false, m_shard_lock);

        if(current)
        {
            m_current_nodes.emplace_back(std::move(current));
            break;
        }
        else
        {
            m_bucket++;
        }
    }

    // at end
    if(m_bucket == NUM_BUCKETS)
    {
        clear();
    }
}

MultiMap::MultiMap(BufferManager &buffer, const std::string &name)
    : AbstractMap(buffer, name)
{
}

MultiMap::~MultiMap() { clear(); }

void MultiMap::find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op)
{
    switch(op)
    {
    case SetOperation::Intersect:
        find_intersect(key, out);
        break;
    case SetOperation::Union:
        find_union(key, out);
        break;
    default:
        abort();
    }
}

void MultiMap::find_intersect(const KeyType &key, std::unordered_set<ValueType> &out)
{
    for(auto it = out.begin(); it != out.end();)
    {
        //FIXME find a faster way to do this?

        bool found = false;
        auto b = to_bucket(key);
        auto &s = get_shard(b);

        ReadLock lock(s);

        auto node = get_node(b, false, lock);
        std::vector<page_no_t> parents;

        while(node && !found)
        {
            if(node->has_entry(key, *it))
            {
                found = true;
                node.clear();
            }
            else
            {
                parents.push_back(node->page_no());
                node = get_successor(b, node, parents, false, lock);
            }
        }
        
        if(found)
        {
            ++it;
        }
        else
        {
            it = out.erase(it);
        }
    }
}

void MultiMap::find_union(const KeyType &key, std::unordered_set<ValueType> &out)
{
    auto b = to_bucket(key);
    auto &s = get_shard(b);

    ReadLock lock(s);

    auto node = get_node(b, false, lock);

    std::vector<page_no_t> parents;

    while(node)
    {
        node->find_union(key, out);
        parents.push_back(node->page_no());
        node = get_successor(b, node, parents, false, lock);
    }
}

size_t MultiMap::estimate_value_count(const KeyType &key)
{
    size_t count = 0;

    auto b = to_bucket(key);
    auto &shard = get_shard(b);
    
    ReadLock lock(shard);

    auto node = get_node(b, false, lock);

    std::vector<page_no_t> parents;

    while(node)
    {
        parents.push_back(node->page_no());

        count += node->estimate_value_count(key);
        node = get_successor(b, node, parents, false, lock);
    }

    return count;
}

bool MultiMap::remove(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto &s = get_shard(b);

    WriteLock lock(s);

    auto node = get_node(b, false, lock, true);

    bool removed = false;

    std::vector<page_no_t> parents;

    while(node && !removed)
    {
        removed = node->remove(key, value);

        if(!removed)
        {
            parents.push_back(node->page_no());
            node = get_successor(b, node, parents, false, lock, true);
        }
    }

    if(removed)
    {
        m_size--;
        return true;
    }
    else
    {
        return false;
    }
}

MultiMap::iterator_t MultiMap::begin() { return { *this, 0 }; }

MultiMap::iterator_t MultiMap::end() { return { *this, NUM_BUCKETS }; }

void MultiMap::insert(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto &s = get_shard(b);

    WriteLock lock(s);

    auto node = get_node(b, true, lock);

    bool created = false;

    std::vector<page_no_t> parents;

    while(!created)
    {
        created = node->insert(key, value);

        if(!created)
        {
            parents.push_back(node->page_no());
            node = get_successor(b, node, parents, true, lock);
        }
    }

    m_size++;
}

void MultiMap::clear()
{
    for(bucketid_t i = 0; i < NUM_BUCKETS; ++i)
    {
        auto &shard = get_shard(i);
        WriteLock lock(shard);

        auto n = get_node(i, false, lock);

        std::vector<page_no_t> parents;

        while(n)
        {
            auto diff = n->clear();
            m_size -= diff;

            parents.push_back(n->page_no());
            n = get_successor(i, n, parents, false, lock);
        }
    }
}

} // namespace trusted
} // namespace credb
