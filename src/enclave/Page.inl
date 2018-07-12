namespace credb
{
namespace trusted
{

inline void Page::flush_page() { m_buffer.flush_page(m_page_no); }
inline void Page::mark_page_dirty() { m_buffer.mark_page_dirty(m_page_no); }

template<typename T>
void PageHandle<T>::clear()
{
    if(m_meta)
    {
        auto &page = *m_meta->page();
        auto &buffer = page.buffer_manager();
        auto page_no = page.page_no();
        buffer.unpin_page(page_no);
        m_meta = nullptr;
    }
}


}
}
