#include "MultiMap.h"
#include "logging.h"
#include "util/get_heap_size_of.h"
#include "util/hash.h"

namespace credb
{
namespace trusted
{

MultiMap::node_t::node_t(BufferManager &buffer, page_no_t page_no)
: Page(buffer, page_no), m_byte_size(0)
{
    m_byte_size += sizeof(*this);
    m_byte_size += get_unordered_container_value_byte_size(m_bucket);
}

MultiMap::node_t::node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
: Page(buffer, page_no), m_byte_size(0)
{
    m_byte_size += sizeof(*this);

    uint32_t bsize;
    bstream >> bsize;
    m_bucket.reserve(bsize);

    for(uint32_t j = 0; j < bsize; ++j)
    {
        KeyType key;
        ValueType value;
        uint32_t vsize;
        bstream >> key >> vsize;

        auto res = m_bucket.emplace(key, std::unordered_set<ValueType>());
        assert(res.second);
        auto &value_set = res.first->second;
        m_byte_size += get_heap_size_of(key);

        for(uint32_t k = 0; k < vsize; ++k)
        {
            bstream >> value;
            value_set.emplace(value);
            m_byte_size += get_heap_size_of(value);
        }
        m_byte_size += get_unordered_container_value_byte_size(value_set);
    }
    m_byte_size += get_unordered_container_value_byte_size(m_bucket);
}

bitstream MultiMap::node_t::serialize() const
{
    bitstream output;
    output << static_cast<uint32_t>(m_bucket.size());
    for(auto map_iter : m_bucket)
    {
        auto &value_set = map_iter.second;
        output << map_iter.first;
        output << static_cast<uint32_t>(value_set.size());
        for(auto &value : value_set)
        {
            output << value;
        }
    }
    return output;
}

bool MultiMap::node_t::remove(const KeyType &key, const ValueType &value)
{
    auto map_iter = m_bucket.find(key);
    if(map_iter == m_bucket.end())
    {
        return false; // not found
    }

    // remove the value
    auto &value_set = map_iter->second;
    m_byte_size -= get_unordered_container_value_byte_size(value_set);
    auto cnt = value_set.erase(value);
    if(cnt)
    {
        m_byte_size -= get_heap_size_of(value);
    }

    m_byte_size += get_unordered_container_value_byte_size(value_set);

    // if nothing left for this key, also remove the set
    if(value_set.empty())
    {
        m_byte_size -= get_unordered_container_value_byte_size(m_bucket);
        m_byte_size -= get_heap_size_of(key);
        m_bucket.erase(map_iter);
        m_byte_size += get_unordered_container_value_byte_size(m_bucket);
    }

    buffer_manager().mark_page_dirty(page_no());
    return cnt;
}

void MultiMap::node_t::find_union(const KeyType &key, std::unordered_set<ValueType> &out)
{
    auto map_iter = m_bucket.find(key);
    if(map_iter == m_bucket.end())
    {
        return;
    }

    auto &value_set = map_iter->second;
    for(auto &value : value_set)
    {
        out.insert(value);
    }
}

void MultiMap::node_t::find_intersect(const KeyType &key, std::unordered_set<ValueType> &out)
{
    auto map_iter = m_bucket.find(key);
    if(map_iter == m_bucket.end())
    {
        out.clear();
        return;
    }

    auto &value_set = map_iter->second;
    for(auto it = out.begin(); it != out.end();)
    {
        if(!value_set.count(*it))
        {
            it = out.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void MultiMap::node_t::find(const KeyType &key, std::unordered_set<ValueType> &out, SetOperation op)
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

size_t MultiMap::node_t::estimate_value_count(const KeyType &key)
{
    // this is not an estimation, it's the correct number
    auto map_iter = m_bucket.find(key);
    if(map_iter == m_bucket.end())
    {
        return 0;
    }
    auto &value_set = map_iter->second;
    return value_set.size();
}

bool MultiMap::node_t::insert(const KeyType &key, const ValueType &value)
{
    auto map_iter = m_bucket.find(key);
    if(map_iter == m_bucket.end())
    {
        // key is not in the bucket
        m_byte_size -= get_unordered_container_value_byte_size(m_bucket);
        auto res = m_bucket.emplace(key, std::unordered_set<ValueType>());
        map_iter = res.first;
        m_byte_size += get_heap_size_of(key);
        m_byte_size += get_unordered_container_value_byte_size(m_bucket);
    }

    auto &value_set = map_iter->second;
    auto set_iter = value_set.find(value);
    if(set_iter != value_set.end())
    {
        // already contains the entry
        return false;
    }

    m_byte_size -= get_unordered_container_value_byte_size(value_set);
    value_set.emplace(value);
    m_byte_size += get_heap_size_of(value);
    m_byte_size += get_unordered_container_value_byte_size(value_set);

    buffer_manager().mark_page_dirty(page_no());
    return true;
}

size_t MultiMap::node_t::byte_size() const { return m_byte_size; }

MultiMap::iterator_t::iterator_t(MultiMap &map, bucketid_t bpos)
: m_map(map), m_bucket(bpos)
{
    next_bucket();
}

MultiMap::iterator_t::~iterator_t() { clear(); }

bool MultiMap::iterator_t::at_end() const { return m_bucket >= NUM_BUCKETS; }

/// Drops all locks and invalidates iterator
void MultiMap::iterator_t::clear()
{
    if(!m_current_node)
    {
        return;
    }

    m_current_node->read_unlock();

    m_current_node.clear();
    m_bucket = NUM_BUCKETS;
}

void MultiMap::iterator_t::operator++()
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    ++m_set_iter;
    if(m_set_iter != m_map_iter->second.cend())
    {
        return;
    }

    ++m_map_iter;
    const auto &bucket = m_current_node->get_bucket();
    if(m_map_iter != bucket.cend())
    {
        m_set_iter = m_map_iter->second.cbegin();
        return;
    }

    ++m_bucket;
    next_bucket();
}

const MultiMap::KeyType &MultiMap::iterator_t::key() const
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    return m_map_iter->first;
}

const MultiMap::ValueType &MultiMap::iterator_t::value() const
{
    if(at_end())
    {
        throw std::runtime_error("Alreayd at end");
    }

    return *m_set_iter;
}

bool MultiMap::iterator_t::operator!=(const iterator_t &other) const
{
    return tie() != other.tie();
}

bool MultiMap::iterator_t::operator==(const iterator_t &other) const
{
    return tie() == other.tie();
}

void MultiMap::iterator_t::next_bucket()
{
    while(m_bucket < NUM_BUCKETS)
    {
        if(m_current_node)
        {
            m_current_node->read_unlock();
        }

        m_current_node = m_map.get_node(m_bucket, LockType::Read);

        const auto &bucket = m_current_node->get_bucket();
        if(!bucket.empty())
        {
            m_map_iter = bucket.cbegin();
            const auto &value_set = m_map_iter->second;
            if(!value_set.empty())
            {
                m_set_iter = value_set.cbegin();
                break;
            }
        }

        m_bucket++;
    }

    if((m_bucket == NUM_BUCKETS) && m_current_node)
    {
        m_current_node->read_unlock();
        m_current_node.clear();
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
    node->find(key, out, op);
    node->read_unlock();
}

size_t MultiMap::estimate_value_count(const KeyType &key)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Read);
    auto res = node->estimate_value_count(key);
    node->read_unlock();
    return res;
}

bool MultiMap::remove(const KeyType &key, const ValueType &value)
{
    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Write);
    bool res = node->remove(key, value);
    node->write_unlock();
    return res;
}

MultiMap::iterator_t MultiMap::begin() { return { *this, 0 }; }

MultiMap::iterator_t MultiMap::end() { return { *this, NUM_BUCKETS }; }

void MultiMap::insert(const KeyType &key, const ValueType &value)
{
    // auto bucket = static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
    // auto block = bucket / buckets_per_node();
    // log_debug("MultiMap::insert: key=" + std::to_string(key) + " value=" + value + " bucket=" +
    // std::to_string(bucket) + " block=" + std::to_string(block));

    auto b = to_bucket(key);
    auto node = get_node(b, LockType::Write);

    bool created = node->insert(key, value);
    if(created)
    {
        std::lock_guard lock(m_size_mutex);
        m_size += 1;
    }

    node->write_unlock();
}

void MultiMap::clear()
{
    std::lock_guard lock(m_size_mutex);
    m_size = 0;
    // TODO
}

PageHandle<MultiMap::node_t> MultiMap::get_node(const bucketid_t bucket, LockType lock_type)
{
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
    auto bucket = static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
    // auto block = bucket / buckets_per_node();
    // log_debug("to_bucket: key=" + std::to_string(key) + " bucket=" + std::to_string(bucket) + "
    // block=" + std::to_string(block));
    return bucket;
}


} // namespace trusted
} // namespace credb
