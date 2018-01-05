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


} // namespace trusted
} // namespace credb
