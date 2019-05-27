/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "HashMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"

namespace credb
{
namespace trusted
{

inline PageHandle<HashMap::node_t> duplicate(PageHandle<HashMap::node_t> &hdl)
{
    if(!hdl)
    {
        return PageHandle<HashMap::node_t>();
    }
    else
    {
        auto &page = *hdl;
        auto &buffer = page.buffer_manager();
        return buffer.get_page<HashMap::node_t>(page.page_no());
    }
}


HashMap::LinearScanKeyProvider::LinearScanKeyProvider(HashMap &index)
    : m_iterator(index, 0)
{
}

bool HashMap::LinearScanKeyProvider::get_next_key(KeyType &key)
{
    if(m_iterator.at_end())
    {
        return false;
    }
    
    key = m_iterator.key();
    ++m_iterator;

    return true;
}

size_t HashMap::LinearScanKeyProvider::count_rest()
{
    auto it2 = m_iterator.duplicate();
    size_t cnt = 0;
    
    while(!it2.at_end())
    {
        ++cnt;
        ++it2;
    }

    return cnt;
}

HashMap::iterator_t::iterator_t(HashMap &map, bucketid_t bpos)
    : m_map(map), m_shard_id(NUM_SHARDS), m_bucket(bpos), m_pos(0)
{
    if(bpos < NUM_BUCKETS)
    {
        next_bucket();
    }
}

HashMap::iterator_t::iterator_t(HashMap &map, bucketid_t bpos, std::vector<PageHandle<node_t>> &current_nodes, uint32_t pos)
    : m_map(map), m_shard_id(bpos % NUM_SHARDS), m_bucket(bpos), m_pos(pos)
{
    m_shard_lock = ReadLock(m_map.get_shard(m_bucket).mutex);

    for(auto &it: current_nodes)
    {
        m_current_nodes.emplace_back(credb::trusted::duplicate(it));
    }
}

HashMap::iterator_t::~iterator_t() { clear(); }

bool HashMap::iterator_t::at_end() const { return m_bucket >= NUM_BUCKETS; }

HashMap::iterator_t HashMap::iterator_t::duplicate()
{
    return HashMap::iterator_t(m_map, m_bucket, m_current_nodes, m_pos);
}

void HashMap::iterator_t::clear()
{
    m_shard_lock.clear();
    m_current_nodes.clear();
    m_bucket = NUM_BUCKETS;
    m_shard_id = NUM_SHARDS;
}

void HashMap::iterator_t::set_value(const ValueType &new_value, bitstream *out_changes)
{
    m_shard_lock.lockable().read_to_write_lock();

    auto rit = m_current_nodes.rbegin();
    auto &current = *rit;
 
    current->insert(key(), new_value);
    current->flush_page();
    ++rit;

    // move up and increment version nos
    for(; rit != m_current_nodes.rend(); ++rit)
    {
        auto &current = *rit;
        current->increment_version_no();
        current->flush_page();
    }

    auto &bucket = m_map.get_bucket(m_bucket);
    bucket.version.increment();

    if(out_changes)
    {
        *out_changes << m_bucket << bucket;
    }

    m_shard_lock.lockable().write_to_read_lock();
}

void HashMap::iterator_t::operator++()
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    if(m_current_nodes.empty())
    {
        throw std::runtime_error("Cannot increment HashMap iterator: invalid state");
    }

    m_pos += 1;
    auto &current = *m_current_nodes.rbegin();
    
    if(m_pos >= current->size())
    {
        auto succ = m_map.get_successor(m_bucket, current, false, m_shard_lock);
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

HashMap::KeyType HashMap::iterator_t::key() const
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }
    
    auto &cur = *m_current_nodes.rbegin();
    auto res = cur->get(m_pos);

    return std::get<0>(res);
}

HashMap::ValueType HashMap::iterator_t::value() const
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    auto &cur = *m_current_nodes.rbegin();
    auto res = cur->get(m_pos);

    return std::get<1>(res);
}

bool HashMap::iterator_t::operator!=(const iterator_t &other) const
{
    return !(*this == other);
}

bool HashMap::iterator_t::operator==(const iterator_t &other) const
{
    return m_bucket == other.m_bucket
        && m_current_nodes.size() == other.m_current_nodes.size()
        && m_pos == other.m_pos;
}

void HashMap::iterator_t::next_bucket()
{
    while(m_bucket < NUM_BUCKETS)
    {
        m_current_nodes.clear();
        auto shard = m_bucket % HashMap::NUM_SHARDS;
        
        if(shard != m_shard_id)
        {
            m_shard_lock.clear();

            m_shard_id = shard;
            m_shard_lock = ReadLock(m_map.get_shard(m_bucket).mutex);
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

HashMap::HashMap(BufferManager &buffer, const std::string &name)
    : AbstractMap(buffer, name)
{
}

HashMap::~HashMap() = default;

HashMap::iterator_t HashMap::begin() { return { *this, 0 }; }

HashMap::iterator_t HashMap::end() { return { *this, NUM_BUCKETS }; }

void HashMap::insert(const KeyType &key, const ValueType &value, bitstream *out_changes)
{
    auto bid = to_bucket(key);
    auto &s = get_shard(bid);

    WriteLock lock(s.mutex);
    auto &bucket = get_bucket(bid);

    auto node = get_node(bid, true, lock);
    bool done = false;

    std::vector<page_no_t> parents;

    while(!done)
    {
        done = node->insert(key, value);

        if(!done)
        {
            parents.push_back(node->page_no());
            node = get_successor(bid, node, true, lock);
        }

        node->flush_page();
    }

    if(out_changes)
    {
        *out_changes << bid << bucket;
    }

    m_size += 1;
}

bool HashMap::get(const KeyType& key, ValueType &value_out)
{
    auto bid = to_bucket(key);
    auto &s = get_shard(bid);
    
    ReadLock lock(s.mutex);

    auto node = get_node(bid, false, lock);

    std::vector<page_no_t> parents;
    while(node)
    {
        if(node->get(key, value_out))
        {
            return true;
        }

        parents.push_back(node->page_no());
        node = get_successor(bid, node, false, lock);
    }

    return false;
}

} // namespace trusted
} // namespace credb
