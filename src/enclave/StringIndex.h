#pragma once
#include <cstdint>
#include <cstring>
#include <unordered_map>

#include "BufferManager.h"
#include "RWLockable.h"
#include "Object.h"
#include "credb/defines.h"

#ifdef TEST
#include <gtest/gtest_prod.h>
#endif

namespace credb
{
namespace trusted
{

typedef uint8_t char_id_t;

constexpr uint8_t CHAR_LO = 46; // .
constexpr uint8_t CHAR_HI = 122; // z
constexpr uint8_t CHAR_ID_SIZE = CHAR_HI - CHAR_LO + 2; // offset 0 is for NULL

constexpr char_id_t INVALID_CHAR = 0;

// Allows to control the branching factor / size of the nodes.
// We found that small nodes is better for performance and memory usage
constexpr uint32_t NODE_DEPTH = 1;

class StringIndexException : public std::exception
{
    std::string m_what;

public:
    /// Constructor
    StringIndexException(const std::string &what) : m_what(what) {}

    /// Get the exception description
    virtual const char *what() const throw() { return m_what.c_str(); }
};

class StringIndex
{
public:
    typedef uint32_t node_version_t;
    static constexpr node_version_t INVALID_VERSION = 0;

    class node_t : public Page, public RWLockable
    {
    public:
        node_t(BufferManager &buffer, page_no_t page_no, node_version_t version);
        node_t(BufferManager &buffer, page_no_t page_no, bitstream &bstream);

        bitstream serialize() const override;
        size_t byte_size() const override;

        node_version_t version() const { return m_version; }
        page_no_t child(char_id_t cid) const { return m_children[cid]; }
        node_version_t child_version(char_id_t cid) const { return m_versions[cid]; }
        event_id_t object(char_id_t cid) const { return m_objects[cid]; }

        void set_child(char_id_t cid, page_no_t page_no);
        void set_child_version(char_id_t cid, node_version_t version);
        void set_version(node_version_t version);
        void set_object(char_id_t cid, const event_id_t &eid);
        void increase_child_version(char_id_t cid);

    private:
        node_version_t m_version;
        page_no_t m_children[CHAR_ID_SIZE];
        node_version_t m_versions[CHAR_ID_SIZE];
        event_id_t m_objects[CHAR_ID_SIZE];
    };

    class iterator_t
    {
    private:
        struct stack_item_t
        {
            PageHandle<node_t> node;
            char_id_t pos;
        };

    public:
        iterator_t(StringIndex &index);
        iterator_t(StringIndex &index, const std::string &key);
        ~iterator_t();
        iterator_t(iterator_t &&other) noexcept;
        void operator++();
        event_id_t value() const;
        std::string key() const;
        void set_value(const event_id_t &value, bitstream *out_changes);
        void clear();
        bool at_end() const;

    private:
        void advance_pos();
        bool has_valid_pos() const;

        friend class StringIndex;
        StringIndex &m_index;
        std::vector<stack_item_t> m_pos_stack;
    };

    class LinearScanKeyProvider : public ObjectKeyProvider
    {
    public:
        LinearScanKeyProvider(StringIndex &index);
        LinearScanKeyProvider(const ObjectListIterator &other) = delete;
        LinearScanKeyProvider(ObjectListIterator &&other) = delete;

        bool get_next_key(std::string &identifier) override;
        size_t count_rest() override;

    private:
        StringIndex &m_index;

        bool m_done;
        std::string m_pos;
    };

    StringIndex(BufferManager &buffer, const std::string &name);
    ~StringIndex();
    PageHandle<node_t> get_node(page_no_t page_no, node_version_t expected_version, LockType lock_type) const;
    PageHandle<node_t> generate_node(node_version_t version, LockType lock_type) const;

    page_no_t root_page_no() const;
    bool get(const std::string &key, event_id_t &val);
    iterator_t begin();
    iterator_t begin_at(const std::string &str);
    void insert(const std::string &key, const event_id_t &val, bitstream *out_changes = nullptr);
    void apply_changes(bitstream &changes);
    const std::string &name() const;
    void reload_root_node();

    void unload_everything(); // for debug purpose
    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

private:
    BufferManager &m_buffer;
    const std::string m_name;
    PageHandle<node_t> m_root_node;
    page_no_t m_root_page_no;
    // TODO: staleness attack of the root node

#ifdef TEST
    FRIEND_TEST(StringIndexTest, string_index_staleness_attack_children);
    FRIEND_TEST(StringIndexTest, string_index_staleness_attack_object);
#endif
};

} // namespace trusted
} // namespace credb
