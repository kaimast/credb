#include "MultiMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"
#include "util/hash.h"

namespace credb
{
namespace trusted
{

MultiMap::node_t::node_t(BufferManager &buffer, page_no_t page_no)
: Page(buffer, page_no)
{
    header_t header = {INVALID_PAGE_NO, 0};
    m_data << header;
}

MultiMap::node_t::node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
: Page(buffer, page_no)
{
    uint8_t *buf;
    uint32_t len;
    bstream.detach(buf, len);
    m_data.assign(buf, len, false);
    m_data.move_to(m_data.size());
}

bitstream MultiMap::node_t::serialize() const
{
    bitstream bstream;
    bstream.assign(m_data.data(), m_data.size(), true);
    return bstream;
}

size_t MultiMap::node_t::clear()
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;

    auto count = header.size;

    auto end = m_data.pos();
    m_data.move_to(sizeof(header));
    m_data.remove_space(end - m_data.pos());

    header.size = 0;
    sview.move_to(0);
    sview << header;

    mark_page_dirty();
    return count;
}

bool MultiMap::node_t::remove(const KeyType &key, const ValueType &value, BufferManager &buffer)
{
    auto old_pos = m_data.pos();

    bitstream view;
    view.assign(m_data.data(), m_data.size(), true);
    view.move_by(sizeof(header_t));

    while(!view.at_end())
    {
        auto pos = view.pos();

        KeyType k;
        ValueType v;

        view >> k >> v;

        auto end = view.pos();

        if(key == k && value == v)
        {
            m_data.move_to(pos);
            auto size = end - pos;
            m_data.remove_space(size);
            m_data.move_to(old_pos - size);

            bitstream sview;
            sview.assign(m_data.data(), m_data.size(), true);
            header_t header;
            sview >> header;
            header.size -= 1;
            sview.move_to(0);
            sview << header;

            mark_page_dirty();
            return true;
        }
    }

    m_data.move_to(old_pos);

    auto succ = get_successor(LockType::Write, false, buffer);

    if(succ)
    {
        auto res = succ->remove(key, value, buffer);
        succ->write_unlock();
        return res;
    }
    else
    {
        return false;
    }
}

size_t MultiMap::node_t::size() const
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;
    return header.size;
}

std::pair<MultiMap::KeyType, MultiMap::ValueType> MultiMap::node_t::get(size_t pos) const
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

void MultiMap::node_t::find_union(const KeyType &key, std::unordered_set<ValueType> &out)
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
            out.insert(v);
        }
    }
}

bool MultiMap::node_t::has_entry(const KeyType &key, const ValueType &value)
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

size_t MultiMap::node_t::estimate_value_count(const KeyType &key)
{
    // this is not an estimation, it's the correct number
    size_t count = 0;

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
            count += 1;
        }
    }

    return count;
}

bool MultiMap::node_t::insert(const KeyType &key, const ValueType &value)
{
    //TODO check if entry already exists? 
    
    if(byte_size() >= MultiMap::MAX_NODE_SIZE)
    {
        return false;
    }
    
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

PageHandle<MultiMap::node_t> MultiMap::node_t::get_successor(LockType lock_type, bool create, BufferManager &buffer)
{
    bitstream sview;
    sview.assign(m_data.data(), m_data.size(), true);
    header_t header;
    sview >> header;

    if(header.successor == INVALID_PAGE_NO)
    {
        if(!create)
        {
            return PageHandle<MultiMap::node_t>();
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

size_t MultiMap::node_t::byte_size() const
{
    return m_data.size() + sizeof(*this);
}

MultiMap::iterator_t::iterator_t(MultiMap &map, bucketid_t bpos)
: m_map(map), m_bucket(bpos), m_pos(0)
{
    if(bpos < NUM_BUCKETS)
    {
        next_bucket();
    }
}

MultiMap::iterator_t::~iterator_t() { clear(); }

bool MultiMap::iterator_t::at_end() const { return m_bucket >= NUM_BUCKETS; }

void MultiMap::iterator_t::clear()
{
    for(auto &node: m_current_nodes)
    {
        node->read_unlock();
    }

    m_current_nodes.clear();
    m_bucket = NUM_BUCKETS;
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
    auto &current = *m_current_nodes.rbegin();

    if(m_pos >= current->size())
    {
        auto succ = current->get_successor(LockType::Read, false, m_map.m_buffer);

        if(succ)
        {
            m_current_nodes.emplace_back(std::move(succ));
            m_pos = 0;
        }
        else
        {
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
    m_bucket++;

    while(m_bucket < NUM_BUCKETS)
    {
        for(auto &node: m_current_nodes)
        {
            node->read_unlock();
        }
        m_current_nodes.clear();

        auto current = m_map.get_node(m_bucket, LockType::Read, false);
        bool empty = true;
        
        if(current)
        {
            empty = current->empty();
            m_current_nodes.emplace_back(std::move(current));
        }

        m_pos = 0;

        if(!empty)
        {
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


MultiMap::MultiMap(BufferManager &buffer, const std::string &name)
    : m_buffer(buffer), m_name(name), m_size(0)
{
    std::fill_n(m_buckets, NUM_BUCKETS, INVALID_PAGE_NO);
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
        auto node = get_node(b, LockType::Read, false);

        while(node && !found)
        {
            if(node->has_entry(key, *it))
            {
                found = true;
            }

            auto succ = node->get_successor(LockType::Read, false, m_buffer);
            node->read_unlock();
            node = std::move(succ);
        }
        
        if(!found)
        {
            it = out.erase(it);
        }
    }
}

void MultiMap::find_union(const KeyType &key, std::unordered_set<ValueType> &out)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Read, false);

    while(node)
    {
        node->find_union(key, out);

        auto succ = node->get_successor(LockType::Read, false, m_buffer);
        node->read_unlock();
        node = std::move(succ);
    }
}

size_t MultiMap::estimate_value_count(const KeyType &key)
{
    size_t count = 0;

    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Read, false);

    while(node)
    {
        count += node->estimate_value_count(key);
        auto succ = node->get_successor(LockType::Read, false, m_buffer);

        node->read_unlock();
        node = std::move(succ);
    }

    return count;
}

bool MultiMap::remove(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Write, false);

    if(node)
    {
        bool res = node->remove(key, value, m_buffer);
        m_size -= res ? 1 : 0;
        node->write_unlock();
        return res;
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
    auto node = get_node(b, LockType::Write, true);

    bool created = false;

    while(!created)
    {
        created = node->insert(key, value);

        if(!created)
        {
            auto succ = node->get_successor(LockType::Write, true, m_buffer);
            node->write_unlock();
            node = std::move(succ);
        }
    }

    m_size += 1;
    node->write_unlock();
}

void MultiMap::clear()
{
    for(bucketid_t i = 0; i < NUM_BUCKETS; ++i)
    {
        auto n = get_node(i, LockType::Write, false);

        while(n)
        {
            auto diff = n->clear();
            m_size -= diff;

            auto succ = n->get_successor(LockType::Write, false, m_buffer);
            n->write_unlock();
            n = std::move(succ);
        }
    }
}

PageHandle<MultiMap::node_t> MultiMap::get_node(const bucketid_t bucket, LockType lock_type, bool create)
{
    std::lock_guard<credb::Mutex> lock(m_node_mutex);
    
    auto &page_no = m_buckets[bucket];
    PageHandle<node_t> node;
    if(page_no == INVALID_PAGE_NO)
    {
        if(!create)
        {
            return PageHandle<MultiMap::node_t>();
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

size_t MultiMap::size() const { return m_size; }

void MultiMap::dump_metadata(bitstream &output) { output << m_buckets << m_size; }

void MultiMap::load_metadata(bitstream &input) { input >> m_buckets >> m_size; }

MultiMap::bucketid_t MultiMap::to_bucket(const KeyType key) const
{
    return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
}


} // namespace trusted
} // namespace credb
