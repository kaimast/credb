namespace credb
{
namespace trusted
{

template <class T, class... Args>
PageHandle<T> BufferManager::shard_t::new_page(page_no_t page_no, Args &&... args)
{
    m_lock.write_lock();

    check_evict();

    auto page = new T(m_buffer, page_no, std::forward<Args>(args)...);
    auto meta = new internal_page_meta_t(page_no, page);
    m_loaded_size += meta->size();
    meta->evict_list_iter = m_evict_list.end();
    meta->mark_dirty();
    m_metas[page_no] = meta;

    m_lock.write_to_read_lock();
    auto handle = get_page_internal<T>(page_no, true);
    m_lock.read_unlock();

    return handle;
}

template <class T>
PageHandle<T> BufferManager::shard_t::get_page_internal(page_no_t page_no, bool load)
{
    if(page_no == INVALID_PAGE_NO)
    {
        return PageHandle<T>();
    }

    auto it = m_metas.find(page_no);
    internal_page_meta_t *meta = nullptr;

    if(it != m_metas.end())
    {
        meta = it->second;
    }
    else
    {
        if(!load)
        {
            // not loaded
            return PageHandle<T>();
        }

        m_lock.read_to_write_lock();

        check_evict();

        bitstream bstream = m_buffer.read_from_disk(page_no);
        auto page = new T(m_buffer, page_no, bstream);
        meta = new internal_page_meta_t(page_no, page);
        meta->evict_list_iter = m_evict_list.end();

        // double check, in case of another thread has loaded it just now
        it = m_metas.find(page_no);
        if(it != m_metas.end())
        {
            delete meta;
            delete page;
            meta = it->second;
            page = static_cast<T*>(meta->page());
        }
        else
        {
            m_loaded_size += meta->size();
            m_metas[page_no] = meta;
        }
        m_lock.write_to_read_lock();
    }

    pin_page(*meta);
    return PageHandle<T>(meta);
}

template<typename T> 
void BufferManager::shard_t::reload_page(page_no_t page_no)
{
    m_lock.read_lock();
    auto it = m_metas.find(page_no);
    assert(it != m_metas.end());
    auto &meta = *it->second;
    m_lock.read_unlock();

    {
        //TODO come up with a better way to handle multiple threads wanting to reload the same page
        std::unique_lock lock(m_reload_mutex);

        bitstream bstream = m_buffer.read_from_disk(page_no);
        auto page = new T(m_buffer, page_no, bstream);
        meta.update_page(page);

        m_loaded_size -= meta.size();
        meta.set_size(page->byte_size());
        m_loaded_size += meta.size();
    }
}


inline void BufferManager::discard_cache(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->discard_cache(page_no);
}

template<typename T>
void BufferManager::reload_page(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->reload_page<T>(page_no);
}

inline void BufferManager::discard_all_cache()
{
    for(auto &shard : m_shards)
    {
        shard->discard_all_cache();
    }
}

inline void BufferManager::flush_all_pages()
{
    for(auto &shard : m_shards)
    {
        shard->flush_all_pages();
    }
}

inline void BufferManager::mark_page_dirty(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->mark_page_dirty(page_no);
}

inline void BufferManager::flush_page(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->flush_page(page_no);
}

inline void BufferManager::unpin_page(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->unpin_page(page_no);
}

}
}

