#include "MultiMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"
#include "util/hash.h"

#include "credb/defines.h"

namespace credb
{
namespace trusted
{

PageHandle<MultiMap::node_t> MultiMap::get_successor(PageHandle<MultiMap::node_t> &prev, bool create, RWHandle &shard_lock, bool modify)
{
    if(!prev)
    {
        return PageHandle<MultiMap::node_t>();
    }

    auto succ = prev->successor();
    auto node = get_node_internal(succ, create, prev->successor_version(), shard_lock);

    if(create)
    {
        if(succ == INVALID_PAGE_NO)
        {
            prev->set_successor(node->page_no());
        }
    }

    if(create || modify)
    {
        prev->increment_version_no();
    }

    return node;
} 

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

PageHandle<MultiMap::node_t> MultiMap::get_node_internal(const page_no_t page_no, bool create, version_number expected_version, RWHandle &shard_lock)
{
    if(page_no == INVALID_PAGE_NO)
    {
        if(create)
        {
            return m_buffer.new_page<node_t>();
        }
        else
        {
            return PageHandle<node_t>();
        }
    }

    while(true)
    {
        auto node = m_buffer.get_page<node_t>(page_no);

        if(node->version_no() != expected_version)
        {
            if(m_buffer.get_encrypted_io().is_remote())
            {
                // The remote party might be in the process of updating it
                // Notifications are always sent after updating the files on disk, so it safe to wait here
                
                node.clear();

                m_bucket_cond.wait(shard_lock);
            }
            else
            {
                auto msg = "Staleness detected! MultiMap node: " + std::to_string(page_no) +
                       "  Expected version: " + std::to_string(expected_version) +
                       "  Read: " + std::to_string(node->version_no());
              
                log_error(msg);
                throw StalenessDetectedException(msg);
            }
        }
        else
        {
            return node;
        }
    }
}

PageHandle<MultiMap::node_t> MultiMap::get_node(const bucketid_t bid, bool create, RWHandle &shard_lock, bool modify)
{
    auto &bucket = m_buckets[bid];
    auto node = get_node_internal(bucket.page_no, create, bucket.version, shard_lock);

    if(create || modify)
    {
        bucket.page_no = node->page_no();
        bucket.version.increment();
    }
    
    return node;
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

        auto succ = m_map.get_successor(current, false, m_shard_lock);
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
            m_shard_lock = ReadLock(m_map.m_shards[m_shard_id]);
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

MultiMap::MultiMap(BufferManager &buffer)
    : m_buffer(buffer), m_size(0)
{
    std::fill_n(m_buckets, NUM_BUCKETS, bucket_t{INVALID_PAGE_NO, 0});
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

        while(node && !found)
        {
            if(node->has_entry(key, *it))
            {
                found = true;
                node.clear();
            }
            else
            {
                node = get_successor(node, false, lock);
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

    while(node)
    {
        node->find_union(key, out);
        node = get_successor(node, false, lock);
    }
}

size_t MultiMap::estimate_value_count(const KeyType &key)
{
    size_t count = 0;

    auto b = to_bucket(key);
    auto &shard = get_shard(b);
    
    ReadLock lock(shard);

    auto node = get_node(b, false, lock);

    while(node)
    {
        count += node->estimate_value_count(key);
        node = get_successor(node, false, lock);
    }

    return count;
}

bool MultiMap::remove(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto &s = m_shards[b % NUM_SHARDS];

    WriteLock lock(s);

    auto node = get_node(b, false, lock, true);

    bool removed = false;

    while(node && !removed)
    {
        removed = node->remove(key, value);

        if(!removed)
        {
            node = get_successor(node, false, lock, true);
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
    auto &s = m_shards[b % NUM_SHARDS];

    WriteLock lock(s);

    auto node = get_node(b, true, lock);

    bool created = false;

    while(!created)
    {
        created = node->insert(key, value);

        if(!created)
        {
            node = get_successor(node, true, lock);
        }
    }

    m_size++;
}

void MultiMap::clear()
{
    for(bucketid_t i = 0; i < NUM_BUCKETS; ++i)
    {
        auto &shard = m_shards[i % NUM_SHARDS];
        ReadLock lock(shard);

        auto n = get_node(i, false, lock);

        while(n)
        {
            auto diff = n->clear();
            m_size -= diff;

            n = get_successor(n, false, lock);
        }
    }
}

size_t MultiMap::size() const { return m_size; }

void MultiMap::dump_metadata(bitstream &output) { output << m_buckets << m_size; }

void MultiMap::load_metadata(bitstream &input) { input >> m_buckets >> m_size; }

MultiMap::bucketid_t MultiMap::to_bucket(const KeyType key) const
{
    return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
}


} // namespace trusted
} // namespace credb
