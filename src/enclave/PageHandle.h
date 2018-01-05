#pragma once

namespace credb
{
namespace trusted
{

template <class T> class PageHandle
{
private:
    T *m_page;

public:
    PageHandle(const PageHandle<T> &other) = delete;
    PageHandle<T> &operator=(const PageHandle<T> &other) = delete;
    PageHandle() : m_page(nullptr) {}

    explicit PageHandle(T &page) : m_page(&page) {}

    PageHandle(PageHandle<T> &&other) : m_page(other.m_page) { other.m_page = nullptr; }

    explicit operator bool() const { return m_page != nullptr; }

    PageHandle<T> &operator=(PageHandle<T> &&other)
    {
        if(this == &other)
        {
            throw std::runtime_error("Cannot move to myself");
        }
        
        clear();
        m_page = other.m_page;
        other.m_page = nullptr;
        return *this;
    }

    ~PageHandle() { clear(); }

    void clear()
    {
        if(m_page)
        {
            auto &buffer = m_page->buffer_manager();
            auto page_no = m_page->page_no();
            buffer.unpin_page(page_no);
            m_page = nullptr;
        }
    }

    T *operator->() const { return m_page; }
    T &operator*() const { return *m_page; }
};

} // namespace trusted
} // namespace credb
