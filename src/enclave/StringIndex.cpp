#include "StringIndex.h"
#include "logging.h"
#include <algorithm>

namespace credb
{
namespace trusted
{

inline char_id_t map_char(char ch)
{
    auto c = static_cast<char_id_t>(ch);
 
    if(c >= CHAR_LO && c <= CHAR_HI)
    {
        return c - CHAR_LO + 1; // offset 0 is for NULL
    }
    else
    {
        throw StringIndexException("Invalid character");
    }
}

inline char unmap_char(char_id_t cid)
{
    return static_cast<char>(cid - 1 + CHAR_LO); // offset 0 is for NULL
}

namespace
{
struct index_update_field_t
{
    bool version : 1;
    bool child : 1;
    bool child_version : 1;
    bool object : 1;
};
} // namespace

//------ StringIndex::node_t

StringIndex::node_t::node_t(BufferManager &buffer, page_no_t page_no, node_version_t version)
: Page(buffer, page_no), m_version(version)
{
    std::fill_n(m_children, CHAR_ID_SIZE, INVALID_PAGE_NO);
    std::fill_n(m_versions, CHAR_ID_SIZE, 0);
    std::fill_n(m_objects, CHAR_ID_SIZE, INVALID_EVENT);
}

StringIndex::node_t::node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
: Page(buffer, page_no)
{
    bstream >> m_version;

    uint8_t *children = nullptr;
    uint8_t *versions = nullptr;
    uint8_t *objects = nullptr;

    bstream.read_raw_data(&children, sizeof(m_children));
    bstream.read_raw_data(&versions, sizeof(m_versions));
    bstream.read_raw_data(&objects, sizeof(m_objects));
    memcpy(m_children, children, sizeof(m_children));
    memcpy(m_versions, versions, sizeof(m_versions));
    memcpy(m_objects, objects, sizeof(m_objects));
}

bitstream StringIndex::node_t::serialize() const
{
    bitstream bstream;
    bstream << m_version;
    bstream.write_raw_data(reinterpret_cast<const uint8_t *>(m_children), sizeof(m_children));
    bstream.write_raw_data(reinterpret_cast<const uint8_t *>(m_versions), sizeof(m_versions));
    bstream.write_raw_data(reinterpret_cast<const uint8_t *>(m_objects), sizeof(m_objects));
    return bstream;
}

size_t StringIndex::node_t::byte_size() const { return sizeof(*this); }

void StringIndex::node_t::set_child(char_id_t cid, page_no_t page_no)
{
    m_children[cid] = page_no;
    mark_page_dirty();
}

void StringIndex::node_t::set_child_version(char_id_t cid, node_version_t version)
{
    m_versions[cid] = version;
    mark_page_dirty();
}

void StringIndex::node_t::set_version(node_version_t version)
{
    m_version = version;
    mark_page_dirty();
}

void StringIndex::node_t::set_object(char_id_t cid, const event_id_t &eid)
{
    m_objects[cid] = eid;
    mark_page_dirty();
}

void StringIndex::node_t::increase_child_version(char_id_t cid)
{
    ++m_versions[cid];
    ++m_version;
    mark_page_dirty();
}


//------ StringIndex::iterator_t

StringIndex::iterator_t::iterator_t(StringIndex &index) : m_index(index)
{
    auto root_id = m_index.root_page_no();
    auto root = m_index.get_node(root_id, 0, LockType::Read);
    m_pos_stack.push_back(stack_item_t{ std::move(root), 1 });

    if(!has_valid_pos())
    {
        advance_pos();
    }
}

StringIndex::iterator_t::iterator_t(StringIndex &index, const std::string &key) : m_index(index)
{
    auto root_id = m_index.root_page_no();
    auto node = m_index.get_node(root_id, 0, LockType::Read);

    for(uint32_t i = 0; i + 1 < key.size(); ++i)
    {
        auto c = map_char(key[i]);
        auto child_id = node->child(c);
        if(child_id == INVALID_PAGE_NO)
        {
            node->read_unlock();
            clear();
            throw StringIndexException("No such entry");
        }

        auto child_version = node->child_version(c);
        m_pos_stack.push_back(stack_item_t{ std::move(node), c });
        auto child = m_index.get_node(child_id, child_version, LockType::Read);
        node = std::move(child);
    }

    auto c = map_char(key.back());
    m_pos_stack.push_back(stack_item_t{ std::move(node), c });
}

StringIndex::iterator_t::~iterator_t() { clear(); }

StringIndex::iterator_t::iterator_t(iterator_t &&other) noexcept
: m_index(other.m_index), m_pos_stack(std::move(other.m_pos_stack))
{
}

void StringIndex::iterator_t::operator++()
{
    if(at_end())
    {
        throw StringIndexException("Already at end");
    }

    advance_pos();
}

event_id_t StringIndex::iterator_t::value() const
{
    auto &top = m_pos_stack.back();
    return top.node->object(top.pos);
}

std::string StringIndex::iterator_t::key() const
{
    std::string res;

    for(auto &it : m_pos_stack)
    {
        res += unmap_char(it.pos);
    }

    return res;
}

void StringIndex::iterator_t::set_value(const event_id_t &value, bitstream *out_changes)
{
    for(size_t i = 0; i + 1 < m_pos_stack.size(); ++i)
    {
        auto &item = m_pos_stack[i];
        auto &node = item.node;
        auto cid = item.pos;

        node->read_to_write_lock();
        node->increase_child_version(cid);
        if(out_changes)
        {
            *out_changes << node->version() << cid << node->child(cid) << node->child_version(cid) << node->object(cid);
        }

        node->flush_page();
        node->write_to_read_lock();
    }

    auto &item = m_pos_stack.back();
    auto &node = item.node;
    auto cid = item.pos;

    node->read_to_write_lock();
    auto child_id = node->child(cid);
    auto child_version = node->child_version(cid);

    node->set_object(cid, value);
    node->increase_child_version(cid);
    if(out_changes)
    {
        *out_changes << node->version() << cid << child_id << child_version + 1 << node->object(cid);
    }

    if(child_id != INVALID_PAGE_NO)
    {
        auto child = m_index.get_node(child_id, child_version, LockType::Write);
        child->set_version(child_version + 1);
        if(out_changes)
        {
            *out_changes << child->version() << INVALID_CHAR << INVALID_PAGE_NO << INVALID_VERSION << INVALID_EVENT;
        }

        child->flush_page();
        child->write_unlock();
    }

    node->flush_page();
    node->write_to_read_lock();
}

void StringIndex::iterator_t::clear()
{
    for(auto it = m_pos_stack.rbegin(); it != m_pos_stack.rend(); ++it)
    {
        it->node->read_unlock();
    }

    m_pos_stack.clear();
}

bool StringIndex::iterator_t::at_end() const { return m_pos_stack.empty(); }

void StringIndex::iterator_t::advance_pos()
{
    do
    {
        auto &top = m_pos_stack.back();

        if(top.pos >= CHAR_ID_SIZE)
        {
            top.node->read_unlock();
            m_pos_stack.pop_back();

            if(!at_end())
            {
                auto &b = m_pos_stack.back();
                b.pos += 1;
            }
            continue;
        }

        auto cid = top.node->child(top.pos);

        if(cid != INVALID_PAGE_NO)
        {
            auto child = m_index.get_node(cid, top.node->child_version(top.pos), LockType::Read);
            m_pos_stack.push_back(stack_item_t{ std::move(child), 1 });
        }
        else
        {
            top.pos += 1;
        }

    } while(!at_end() && !has_valid_pos());
}

bool StringIndex::iterator_t::has_valid_pos() const
{
    auto &top = m_pos_stack.back();

    if(top.pos >= CHAR_ID_SIZE)
    {
        return false;
    }

    return top.node->object(top.pos) != INVALID_EVENT;
}


//------ StringIndex::LinearScanKeyProvider

StringIndex::LinearScanKeyProvider::LinearScanKeyProvider(StringIndex &index)
: m_index(index), m_done(false), m_pos("")
{
    auto it = m_index.begin();
    if(it.at_end())
    {
        m_done = true;
    }
    else
    {
        m_pos = it.key();
    }
}

bool StringIndex::LinearScanKeyProvider::get_next_key(std::string &identifier)
{
    if(m_done)
    {
        return false;
    }

    auto it = m_index.begin_at(m_pos);
    identifier = m_pos;
    ++it;

    if(it.at_end())
    {
        m_done = true;
    }

    m_pos = it.key();
    return true;
}

size_t StringIndex::LinearScanKeyProvider::count_rest()
{
    size_t cnt = 0;
    constexpr int batch_size = 100; // trade off: speed & check_evict
    while(!m_done)
    {
        auto it = m_index.begin_at(m_pos);
        for(int i = 0; i < batch_size; ++i)
        {
            ++cnt;
            ++it;
            if(it.at_end())
            {
                m_done = true;
            }
            m_pos = it.key();
        }
    }
    return cnt;
}


//------ StringIndex

StringIndex::StringIndex(BufferManager &buffer, const std::string &name)
: m_buffer(buffer), m_name(name), m_root_node(m_buffer.new_page<node_t>(1)),
  m_root_page_no(m_root_node->page_no())
{
}

StringIndex::~StringIndex() = default;

page_no_t StringIndex::root_page_no() const { return m_root_page_no; }

PageHandle<StringIndex::node_t>
StringIndex::get_node(page_no_t page_no, node_version_t expected_version, LockType lock_type) const
{
    auto node = m_buffer.get_page<node_t>(page_no);
    node->lock(lock_type);
    if(expected_version != 0 &&
       node->version() < expected_version) // FIXME: is this scheme correct?
    {
        auto msg = "Staleness detected! StringIndex node: " + std::to_string(page_no) +
                   "  Expected version: " + std::to_string(expected_version) +
                   " Read: " + std::to_string(node->version());
        log_error(msg);
        throw StalenessDetectedException(msg);
    }
    return node;
}

PageHandle<StringIndex::node_t> StringIndex::generate_node(node_version_t version, LockType lock_type) const
{
    auto node = m_buffer.new_page<node_t>(version);
    node->lock(lock_type);
    return node;
}

bool StringIndex::get(const std::string &key, event_id_t &val)
{
    try
    {
        auto it = begin_at(key);
        val = it.value();
        return it.has_valid_pos();
    }
    catch(StringIndexException &e)
    {
        return false;
    }
}

StringIndex::iterator_t StringIndex::begin() { return iterator_t(*this); }

StringIndex::iterator_t StringIndex::begin_at(const std::string &str)
{
    if(str.empty())
    {
        return begin();
    }
    else
    {
        return iterator_t(*this, str);
    }
}

void StringIndex::insert(const std::string &key, const event_id_t &val, bitstream *out_changes)
{
    if(key.empty())
    {
        throw StringIndexException("Invalid key");
    }

    auto node = get_node(m_root_page_no, 0, LockType::Write);
    for(uint32_t i = 0; i + 1 < key.size(); ++i)
    {
        auto c = map_char(key[i]);
        auto child_id = node->child(c);
        auto child_version = node->child_version(c);
        PageHandle<node_t> child;

        if(child_id == INVALID_PAGE_NO)
        {
            child = generate_node(child_version, LockType::Write);
            node->set_child(c, child->page_no());
        }
        else
        {
            child = get_node(child_id, child_version, LockType::Write);
        }
        node->increase_child_version(c);

        if(out_changes)
        {
            *out_changes << node->version() << c << node->child(c) << child_version + 1 << node->object(c);
        }

        node->flush_page();
        node->write_unlock();
        node = std::move(child);
    }

    auto c = map_char(key.back());
    auto child_id = node->child(c);
    auto child_version = node->child_version(c);
    node->set_object(c, val);
    node->increase_child_version(c);
    if(out_changes)
    {
        *out_changes << node->version() << c << child_id << child_version + 1 << node->object(c);
    }

    if(child_id != INVALID_PAGE_NO)
    {
        auto child = get_node(child_id, child_version, LockType::Write);
        child->set_version(child_version + 1);
        if(out_changes)
        {
            *out_changes << child->version() << INVALID_CHAR << INVALID_PAGE_NO << INVALID_VERSION << INVALID_EVENT;
        }
        child->flush_page();
        child->write_unlock();
    }

    node->flush_page();
    node->write_unlock();
}

// reminder: sync with StringIndex::insert & StringIndex::iterator_t::set_value
void StringIndex::apply_changes(bitstream &changes)
{
    auto node = get_node(m_root_page_no, 0, LockType::Write);

    while(!changes.at_end())
    {
        char_id_t char_id;
        page_no_t child_id;
        node_version_t node_version, child_version;
        event_id_t object;
        changes >> node_version >> char_id >> child_id >> child_version >> object;

        node->set_version(node_version);
        if(char_id != INVALID_CHAR)
        {
            auto cid = node->child(char_id);
            if(cid != INVALID_PAGE_NO && cid != child_id)
            {
                log_error("FIXME: delete the old child");
                abort();
            }

            node->set_child(char_id, child_id);
            node->set_child_version(char_id, child_version);
            node->set_object(char_id, object);
        }

        if(child_id == INVALID_PAGE_NO)
        {
            break;
        }

        auto child = m_buffer.get_page_if_cached<node_t>(child_id);
        if(!child)
        {
            // not in memory, discard cache
            child.clear();
            m_buffer.discard_cache(child_id);
            while(!changes.at_end())
            {
                changes >> node_version >> char_id >> child_id >> child_version >> object;
                m_buffer.discard_cache(child_id);
            }
        }
        else
        {
            child->write_lock();
            node->write_unlock();
            node = std::move(child);
        }
    }

    node->write_unlock();
}

const std::string &StringIndex::name() const { return m_name; }

void StringIndex::reload_root_node()
{
    m_root_node.clear();
    m_buffer.discard_cache(m_root_page_no);
    m_root_node = m_buffer.get_page<node_t>(m_root_page_no);
}

void StringIndex::unload_everything()
{
    m_root_node.clear();
    m_root_page_no = INVALID_PAGE_NO;
}

void StringIndex::dump_metadata(bitstream &output) { output << m_root_page_no; }

void StringIndex::load_metadata(bitstream &input)
{
    input >> m_root_page_no;
    m_root_node = m_buffer.get_page<node_t>(m_root_page_no);
}

} // namespace trusted
} // namespace credb
