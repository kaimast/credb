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
    return m_data.duplicate(true);
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

void MultiMap::node_t::find_union(const KeyType &key, std::unordered_set<ValueType> &out, BufferManager &buffer)
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

    auto succ = get_successor(LockType::Read, false, buffer);

    if(succ)
    {
        succ->find_union(key, out, buffer);
        succ->read_unlock();
    }

}

void MultiMap::node_t::find_intersect(const KeyType &key, std::unordered_set<ValueType> &out, BufferManager &buffer)
{
    for(auto it = out.begin(); it != out.end();)
    {
        if(has_entry(key, *it, buffer))
        {
            ++it;
        }
        else
        {
            it = out.erase(it);
        }
    }
}

bool MultiMap::node_t::has_entry(const KeyType &key, const ValueType &value, BufferManager &buffer)
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

    auto succ = get_successor(LockType::Read, false, buffer);

    if(succ)
    {
        auto res = succ->has_entry(key, value, buffer);
        succ->read_unlock();
        return res;
    }
    else
    {
        return false;
    }
}

void MultiMap::node_t::find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op, BufferManager &buffer)
{
    switch(op)
    {
    case SetOperation::Intersect:
        find_intersect(key, out, buffer);
        break;
    case SetOperation::Union:
        find_union(key, out, buffer);
        break;
    default:
        abort();
    }
}

size_t MultiMap::node_t::estimate_value_count(const KeyType &key, BufferManager &buffer)
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

    auto succ = get_successor(LockType::Read, false, buffer);

    if(succ)
    {
        count += succ->estimate_value_count(key, buffer);
        succ->read_unlock();
    }

    return count;
}

bool MultiMap::node_t::insert(const KeyType &key, const ValueType &value, BufferManager &buffer)
{
    //TODO check if entry already exists? 
    
    if(byte_size() < MultiMap::MAX_NODE_SIZE)
    {
        m_data << key << value;

        bitstream sview;
        sview.assign(m_data.data(), m_data.size(), true);

        header_t header;
        sview >> header;
        header.size += 1;
        sview.move_to(0);
        sview << header;
        return true;
    }
    else
    {
        auto succ = get_successor(LockType::Write, true, buffer);
        auto res = succ->insert(key, value, buffer);
        succ->write_unlock();
        return res;
    }
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

        auto current = m_map.get_node(m_bucket, LockType::Read);
        const bool empty = current->empty();
        m_current_nodes.emplace_back(std::move(current));
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
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Read);
    node->find(key, out, op, m_buffer);
    node->read_unlock();
}

size_t MultiMap::estimate_value_count(const KeyType &key)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Read);
    auto res = node->estimate_value_count(key, m_buffer);
    node->read_unlock();
    return res;
}

bool MultiMap::remove(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Write);
    bool res = node->remove(key, value, m_buffer);
    m_size -= 1;
    node->write_unlock();
    return res;
}

MultiMap::iterator_t MultiMap::begin() { return { *this, 0 }; }

MultiMap::iterator_t MultiMap::end() { return { *this, NUM_BUCKETS }; }

void MultiMap::insert(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Write);

    bool created = node->insert(key, value, m_buffer);
    if(created)
    {
        m_size += 1;
    }

    node->write_unlock();
}

void MultiMap::clear()
{
    m_size = 0;
    // TODO
}

PageHandle<MultiMap::node_t> MultiMap::get_node(const bucketid_t bucket, LockType lock_type)
{
    std::lock_guard<credb::Mutex> lock(m_node_mutex);
    
    auto &page_no = m_buckets[bucket];
    PageHandle<node_t> node;
    if(page_no == INVALID_PAGE_NO)
    {
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
