#include "HashMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"
#include "util/hash.h"

namespace credb
{
namespace trusted
{

inline void increment_version(HashMap::version_no_t &vno)
{
    vno = (vno + 1) % UINT_LEAST16_MAX;
}

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

HashMap::node_t::node_t(BufferManager &buffer, page_no_t page_no)
: Page(buffer, page_no)
{
    header_t header = {0, INVALID_PAGE_NO, 0, 0};
    m_data << header;
}

HashMap::node_t::node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
: Page(buffer, page_no)
{
    uint8_t *buf;
    uint32_t len;
    bstream.detach(buf, len);
    m_data.assign(buf, len, false);
    m_data.move_to(m_data.size());
}

bitstream HashMap::node_t::serialize() const
{
    bitstream bstream;
    bstream.assign(m_data.data(), m_data.size(), true);
    return bstream;
}

void HashMap::node_t::increment_version_no()
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;

    increment_version(header.version);
    increment_version(header.successor_version);

    sview.move_to(0);
    sview << header;

    mark_page_dirty();
}

HashMap::version_no_t HashMap::node_t::version_no() const
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;
    return header.version;
}

size_t HashMap::node_t::size() const
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;
    return header.size;
}

std::pair<HashMap::KeyType, HashMap::ValueType> HashMap::node_t::get(size_t pos) const
{
    bitstream view;
    view.assign(m_data.data(), m_data.size(), true);
    view.move_by(sizeof(header_t));

    size_t cpos = 0;

    while(!view.at_end())
    {
        KeyType k;
        ValueType v;

        view >> k >> v;

        if(cpos == pos)
        {
            return {k, v};
        }

        cpos += 1;
    }
    
    throw std::runtime_error("Out of bounds!");
}

bool HashMap::node_t::has_entry(const KeyType &key, const ValueType &value)
{
    bitstream view;
    view.assign(m_data.data(), m_data.size(), true);

    view.move_by(sizeof(header_t));

    while(!view.at_end())
    {
        KeyType k;
        ValueType v;

        view >> k >> v;

        if(key == k && value == v)
        {
            return true;
        }
    }
    
    return false;
}

bool HashMap::node_t::insert(const KeyType &key, const ValueType &value)
{
    bitstream view;
    view.assign(m_data.data(), m_data.size(), true);

    view.move_by(sizeof(header_t));

    bool updated = false;

    while(!view.at_end())
    {
        KeyType k;
        view >> k;

        if(key == k)
        {
            view << value;

            mark_page_dirty();
            updated = true;
        }
        else
        {
            ValueType v;
            view >> v;
        }
    }
 
    if(updated || byte_size() < HashMap::MAX_NODE_SIZE)
    {
        if(!updated)
        {
            m_data << key << value;
        }

        bitstream sview;
        sview.assign(m_data.data(), m_data.size(), true);

        header_t header;
        sview >> header;

        if(!updated)
        {
            header.size += 1;
        }
    
        increment_version(header.version);

        sview.move_to(0);
        sview << header;

        mark_page_dirty();
        return true;
    }
    else
    {
        return false;
    }
}

bool HashMap::node_t::get(const KeyType& key, ValueType &value_out)
{
    bitstream view;
    view.assign(m_data.data(), m_data.size(), true);

    view.move_by(sizeof(header_t));

    while(!view.at_end())
    {
        KeyType k;
        ValueType v;

        view >> k >> v;

        if(key == k)
        {
            value_out = v;
            return true;
        }
    }
    
    return false;
}

PageHandle<HashMap::node_t> HashMap::node_t::get_successor(bool create, BufferManager &buffer)
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;

    PageHandle<HashMap::node_t> res;

    if(header.successor == INVALID_PAGE_NO)
    {
        if(create)
        {
            res = buffer.new_page<node_t>();
            header.successor = res->page_no();

            sview.move_to(0);
            sview << header;

            mark_page_dirty();
        }
        else
        {
            return PageHandle<HashMap::node_t>();
        }
    }
    else
    {
        res = buffer.get_page<node_t>(header.successor);

        if(res->version_no() != header.successor_version)
        {
            auto msg = "Staleness detected! HashMap node: " + std::to_string(res->page_no()) +
                       "  Expected version: " + std::to_string(header.successor_version) +
                       " Read: " + std::to_string(res->version_no());
            log_error(msg);
            throw StalenessDetectedException(msg);
        }
    }

    if(create)
    {
        this->increment_version_no();
    }

    return res;
} 

size_t HashMap::node_t::byte_size() const
{
    return m_data.size() + sizeof(*this);
}

HashMap::iterator_t::iterator_t(HashMap &map, bucketid_t bpos)
    : m_map(map), m_shard(NUM_SHARDS), m_bucket(bpos), m_pos(0)
{
    if(bpos < NUM_BUCKETS)
    {
        next_bucket();
    }
}

HashMap::iterator_t::iterator_t(HashMap &map, bucketid_t bpos, std::vector<PageHandle<node_t>> &current_nodes, uint32_t pos)
    : m_map(map), m_shard(bpos % NUM_SHARDS), m_bucket(bpos), m_pos(pos)
{
    m_map.m_shards[m_shard].read_lock();

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
    if(m_shard < NUM_SHARDS)
    {
        m_map.m_shards[m_shard].read_unlock();
    }

    m_current_nodes.clear();
    m_bucket = NUM_BUCKETS;
    m_shard = NUM_SHARDS;
}

void HashMap::iterator_t::set_value(const ValueType &new_value)
{
    auto &s = m_map.m_shards[m_shard];
    s.read_to_write_lock();

    auto rit = m_current_nodes.rbegin();
    auto &current = *rit;
 
    current->insert(key(), new_value);

    ++rit;

    // move up and increment version nos
    for(; rit != m_current_nodes.rend(); ++rit)
    {
        auto &current = *rit;
        current->increment_version_no();
    }

    auto &bucket = m_map.m_buckets[m_bucket];
    increment_version(bucket.version);

    s.write_to_read_lock();
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
        auto succ = current->get_successor(false, m_map.m_buffer);
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

        auto current = m_map.get_node(m_bucket, false);

        auto shard = m_bucket % HashMap::NUM_SHARDS;
        
        if(shard != m_shard)
        {
            if(m_shard < NUM_SHARDS)
            {
                m_map.m_shards[m_shard].read_unlock();
            }

            m_shard = shard;
            m_map.m_shards[m_shard].read_lock();
        }

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

void HashMap::insert(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto &s = m_shards[b % NUM_SHARDS];

    s.write_lock();

    auto node = get_node(b, true);

    bool done = false;

    while(!done)
    {
        done = node->insert(key, value);

        if(!done)
        {
            auto succ = node->get_successor(true, m_buffer);
            node = std::move(succ);
        }
    }

    m_size += 1;

    s.write_unlock();
}

bool HashMap::get(const KeyType& key, ValueType &value_out)
{
    auto bid = to_bucket(key);
    auto &s = m_shards[bid % NUM_SHARDS];
    
    s.read_lock();
    auto node = get_node(bid, false);

    while(node)
    {
        if(node->get(key, value_out))
        {
            s.read_unlock();
            return true;
        }

        node = node->get_successor(false, m_buffer);
    }

    s.read_unlock();
    return false;
}

PageHandle<HashMap::node_t> HashMap::get_node(const bucketid_t bid, bool create)
{
    std::lock_guard<credb::Mutex> lock(m_node_mutex);
    
    auto &bucket = m_buckets[bid];

    PageHandle<node_t> node;

    if(bucket.page_no == INVALID_PAGE_NO)
    {
        if(create)
        {
            node = m_buffer.new_page<node_t>();
            bucket.page_no = node->page_no();
        }
        else
        {
            return PageHandle<HashMap::node_t>();
        }
    }
    else
    {
        node = m_buffer.get_page<node_t>(bucket.page_no);

        if(node->version_no() != bucket.version)
        {
            auto msg = "Staleness detected! StringIndex node: " + std::to_string(bucket.page_no) +
                       "  Expected version: " + std::to_string(bucket.version) +
                       " Read: " + std::to_string(node->version_no());
            log_error(msg);
            throw StalenessDetectedException(msg);
        }
    }

    if(create)
    {
        increment_version(bucket.version);
    }

    return node;
}

size_t HashMap::size() const { return m_size; }

HashMap::bucketid_t HashMap::to_bucket(const KeyType key) const
{
    return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
}


} // namespace trusted
} // namespace credb
