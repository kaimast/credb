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
    //return PageHandle<T>(*m_page);
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
    header_t header = {INVALID_PAGE_NO, 0};
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

    while(!view.at_end())
    {
        KeyType k;
        view >> k;

        if(key == k)
        {
            view << value;

            mark_page_dirty();
            return true;
        }
        else
        {
            ValueType v;
            view >> v;
        }
    }
 
    if(byte_size() >= HashMap::MAX_NODE_SIZE)
    {
        return false;
    }
    else
    {
        m_data << key << value;

        bitstream sview;
        sview.assign(m_data.data(), m_data.size(), true);

        header_t header;
        sview >> header;
        header.size += 1;
        sview.move_to(0);
        sview << header;

        mark_page_dirty();
        return true;
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

PageHandle<HashMap::node_t> HashMap::node_t::get_successor(LockType lock_type, bool create, BufferManager &buffer)
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;

    if(header.successor == INVALID_PAGE_NO)
    {
        if(!create)
        {
            return PageHandle<HashMap::node_t>();
        }

        auto res = buffer.new_page<node_t>();
        header.successor = res->page_no();
        res->lock(lock_type);

        sview.move_to(0);
        sview << header;

        mark_page_dirty();
        return res;
    }
    else
    {
        auto res = buffer.get_page<node_t>(header.successor);
        res->lock(lock_type);
        return res;
    }
} 

size_t HashMap::node_t::byte_size() const
{
    return m_data.size() + sizeof(*this);
}

HashMap::iterator_t::iterator_t(HashMap &map, bucketid_t bpos)
    : m_map(map), m_bucket(bpos), m_pos(0)
{
    if(bpos < NUM_BUCKETS)
    {
        next_bucket();
    }
}

HashMap::iterator_t::iterator_t(HashMap &map, bucketid_t bpos, std::vector<PageHandle<node_t>> &current_nodes, uint32_t pos)
    : m_map(map), m_bucket(bpos), m_pos(pos)
{
    for(auto &it: current_nodes)
    {
        it->read_lock();
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
    for(auto &node: m_current_nodes)
    {
        node->read_unlock();
    }

    m_current_nodes.clear();
    m_bucket = NUM_BUCKETS;
}

void HashMap::iterator_t::set_value(const ValueType &new_value)
{
    auto &current = *m_current_nodes.rbegin();
 
    current->read_to_write_lock();
    current->insert(key(), new_value);
    current->write_to_read_lock();
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
        auto succ = current->get_successor(LockType::Read, false, m_map.m_buffer);
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
        for(auto &node: m_current_nodes)
        {
            node->read_unlock();
        }
        m_current_nodes.clear();

        auto current = m_map.get_node(m_bucket, LockType::Read, false);

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
        for(auto &node: m_current_nodes)
        {
            node->read_unlock();
        }

        m_current_nodes.clear();
    }
}

HashMap::HashMap(BufferManager &buffer, const std::string &name)
    : m_buffer(buffer), m_name(name), m_size(0)
{
    std::fill_n(m_buckets, NUM_BUCKETS, INVALID_PAGE_NO);
}

HashMap::~HashMap() = default;

HashMap::iterator_t HashMap::begin() { return { *this, 0 }; }

HashMap::iterator_t HashMap::end() { return { *this, NUM_BUCKETS }; }

void HashMap::insert(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Write, true);

    bool done = false;

    while(!done)
    {
        done = node->insert(key, value);

        if(!done)
        {
            auto succ = node->get_successor(LockType::Write, true, m_buffer);
            node->write_unlock();
            node = std::move(succ);
        }
    }

    m_size += 1;
    node->write_unlock();
}

bool HashMap::get(const KeyType& key, ValueType &value_out)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Read, false);

    while(node)
    {
        if(node->get(key, value_out))
        {
            node->read_unlock();
            return true;
        }

        auto succ = node->get_successor(LockType::Read, false, m_buffer);
        node->read_unlock();
        node = std::move(succ);
    }

    return false;
}

PageHandle<HashMap::node_t> HashMap::get_node(const bucketid_t bucket, LockType lock_type, bool create)
{
    std::lock_guard<credb::Mutex> lock(m_node_mutex);
    
    auto &page_no = m_buckets[bucket];
    PageHandle<node_t> node;
    if(page_no == INVALID_PAGE_NO)
    {
        if(!create)
        {
            return PageHandle<HashMap::node_t>();
        }

        node = m_buffer.new_page<node_t>();
        page_no = node->page_no();
    }
    else
    {
        node = m_buffer.get_page<node_t>(page_no);
    }

    node->lock(lock_type);
    return node;
}

size_t HashMap::size() const { return m_size; }

HashMap::bucketid_t HashMap::to_bucket(const KeyType key) const
{
    return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
}


} // namespace trusted
} // namespace credb
