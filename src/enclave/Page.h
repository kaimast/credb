#pragma once

#include "util/defines.h"
#include <bitstream.h>

namespace credb
{
namespace trusted
{

class BufferManager;

class Page
{
public:
    Page(BufferManager &buffer, page_no_t page_no) : m_buffer(buffer), m_page_no(page_no) {}
    virtual ~Page() = default;

    BufferManager &buffer_manager() const { return m_buffer; }
    page_no_t page_no() const { return m_page_no; }
    void flush_page();
    void mark_page_dirty();

    // abstract functions
    virtual bitstream serialize() const = 0;
    virtual size_t byte_size() const = 0;

    // a derived class T needs to have the following constructor
    // T(BufferManager& buffer, page_no_t page_no, ...);
    // T(BufferManager& buffer, page_no_t page_no, bitstream &bstream);

private:
    BufferManager &m_buffer;
    page_no_t m_page_no;
};

struct internal_page_meta_t : public credb::Mutex
{
private:
    page_no_t m_page_no;
    Page *m_page;
    bool m_dirty;
    size_t m_size;

public:
    page_no_t page_no() { return m_page_no; }
    Page* page() { return m_page; }

    bool dirty() const { return m_dirty; } 
    void mark_dirty() { m_dirty = true; }
    void unmark_dirty() { m_dirty = false; }
    
    size_t size() const { return sizeof(*this) + m_size; }
    void set_size(size_t new_size) { m_size = new_size; }

    size_t cnt_pin;
    std::list<internal_page_meta_t *>::iterator evict_list_iter;

    internal_page_meta_t(page_no_t _page_no, Page *page_)
    : m_page_no(_page_no), m_page(page_), m_dirty(false), m_size(page()->byte_size()), cnt_pin(0) 
    {
    }

    void update_page(Page *new_page)
    {
        delete m_page;
        m_page = new_page;
    }
};

} // namespace trusted
} // namespace credb
