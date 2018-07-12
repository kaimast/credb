#pragma once

#include "Page.h"

namespace credb
{
namespace trusted
{

template <class T> class PageHandle
{
private:
    internal_page_meta_t *m_meta;

public:
    PageHandle(const PageHandle<T> &other) = delete;
    PageHandle<T>& operator=(const PageHandle<T> &other) = delete;

    PageHandle() : m_meta(nullptr) {}

    explicit PageHandle(internal_page_meta_t *meta)
        : m_meta(meta)
    {}

    PageHandle(PageHandle<T> &&other)
       : m_meta(other.m_meta)
    { other.m_meta = nullptr; }

    explicit operator bool() const { return m_meta != nullptr; }

    PageHandle<T> &operator=(PageHandle<T> &&other)
    {
        if(this == &other)
        {
            throw std::runtime_error("Cannot move to myself");
        }

        clear();
        
        m_meta = other.m_meta;
        other.m_meta = nullptr;
        return *this;
    }

    ~PageHandle()
    {
        clear();
    }

    void clear();

    T *operator->() const { return dynamic_cast<T*>(m_meta->page()); }
    T &operator*() const { return dynamic_cast<T&>(*m_meta->page()); }
};

} // namespace trusted
} // namespace credb
