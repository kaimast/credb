#include "HashMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"
#include "util/hash.h"

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

void HashMap::apply_changes(bitstream &changes)
{
    bucketid_t bid;
    bucket_t new_val;
    changes >> bid >> new_val;

    auto &bucket = m_buckets[bid];

    if(new_val.version < bucket.version)
    {
        // outdated update
        return;
    }

    auto &s = m_shards[bid % NUM_SHARDS];

    WriteLock lock(s);

    // Discard all cached pages of that bucket
    auto current = m_buffer.get_page_if_cached<node_t>(bucket.page_no);

    std::vector<page_no_t> nodes;
    if(current)
    {
        nodes.push_back(current->page_no());
    }

    while(current)
    {
        auto succ = current->successor();
        nodes.push_back(current->page_no());
        current = m_buffer.get_page_if_cached<node_t>(succ);
    }

    for(auto node: nodes)
    {
        m_buffer.discard_cache(node);
    }

    bucket = new_val;

    m_bucket_cond.notify_all();
}

PageHandle<HashMap::node_t> HashMap::get_successor(PageHandle<HashMap::node_t> &prev, bool create, RWHandle &shard_lock)
{
    auto succ = prev->successor();

    PageHandle<node_t> node = get_node_internal(succ, create, prev->successor_version(), shard_lock);

    if(create)
    {
        if(succ == INVALID_PAGE_NO)
        {
            prev->set_successor(node->page_no());
        }

        prev->increment_version_no();
    }

    return node;
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
    m_shard_lock = ReadLock(m_map.m_shards[m_shard_id]);

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

    auto &bucket = m_map.m_buckets[m_bucket];
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
        throw std::runtime_error("Invalid state");
    }

    m_pos += 1;
    auto &current = *m_current_nodes.rbegin();
    
    if(m_pos >= current->size())
    {
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

HashMap::HashMap(BufferManager &buffer, const std::string &name)
    : m_buffer(buffer), m_name(name), m_size(0)
{
    std::fill_n(m_buckets, NUM_BUCKETS, bucket_t{INVALID_PAGE_NO, 0});
}

HashMap::~HashMap() = default;

HashMap::iterator_t HashMap::begin() { return { *this, 0 }; }

HashMap::iterator_t HashMap::end() { return { *this, NUM_BUCKETS }; }

void HashMap::insert(const KeyType &key, const ValueType &value, bitstream *out_changes)
{
    auto b = to_bucket(key);
    auto &s = m_shards[b % NUM_SHARDS];

    WriteLock lock(s);

    auto node = get_node(b, true, lock);

    bool done = false;

    while(!done)
    {
        done = node->insert(key, value);

        if(!done)
        {
            auto succ = get_successor(node, true, lock);
            node = std::move(succ);
        }

        node->flush_page();
    }

    if(out_changes)
    {
        *out_changes << b << m_buckets[b];
    }

    m_size += 1;
}

bool HashMap::get(const KeyType& key, ValueType &value_out)
{
    auto bid = to_bucket(key);
    auto &s = m_shards[bid % NUM_SHARDS];
    
    ReadLock lock(s);

    auto node = get_node(bid, false, lock);

    while(node)
    {
        if(node->get(key, value_out))
        {
            return true;
        }

        node = get_successor(node, false, lock);
    }

    return false;
}

PageHandle<HashMap::node_t> HashMap::get_node_internal(const page_no_t page_no, bool create, version_number expected_version, RWHandle &shard_lock)
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
                auto msg = "Staleness detected! HasMap node: " + std::to_string(page_no) +
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

PageHandle<HashMap::node_t> HashMap::get_node(const bucketid_t bid, bool create, RWHandle &shard_lock)
{
    auto &bucket = m_buckets[bid];
    auto node = get_node_internal(bucket.page_no, create, bucket.version, shard_lock);

    if(create)
    {
        bucket.page_no = node->page_no();
        bucket.version.increment();
    }
    
    return node;
}

void HashMap::serialize_root(bitstream &out)
{
    for(auto &shard: m_shards)
    {
        shard.read_lock();
    }

    out << m_buckets;

    for(auto &shard: m_shards)
    {
        shard.read_unlock();
    }
}

void HashMap::load_root(bitstream &in)
{
    for(auto &shard : m_shards)
    {
        shard.write_lock();
    }

    in >> m_buckets;

    for(auto &shard : m_shards)
    {
        shard.write_unlock();
    }
}

size_t HashMap::size() const { return m_size; }

HashMap::bucketid_t HashMap::to_bucket(const KeyType key) const
{
    return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
}


} // namespace trusted
} // namespace credb
