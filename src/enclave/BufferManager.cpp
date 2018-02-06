#include "BufferManager.h"
#include "logging.h"

namespace credb
{
namespace trusted
{

BufferManager::shard_t::~shard_t()
{
    for(auto &it : m_metas)
    {
        auto meta = it.second;
        {
            meta->lock();
            flush_page(*meta);
            meta->unlock();
        }
        delete meta->page;
        delete meta;
    }
}

void BufferManager::shard_t::pin_page(internal_page_meta_t &meta)
{
    ++meta.cnt_pin;
    if(meta.evict_list_iter != m_evict_list.end())
    {
        std::lock_guard evict_lock(m_evict_mutex);
        m_evict_list.erase(meta.evict_list_iter);
        meta.evict_list_iter = m_evict_list.end();
    }
}

void BufferManager::shard_t::unpin_page(page_no_t page_no)
{
    m_lock.read_lock();
    auto it = m_metas.find(page_no);
    assert(it != m_metas.end());
    auto &meta = *it->second;
    m_lock.read_unlock();

    meta.lock();
    assert(meta.cnt_pin > 0);
    if(!--meta.cnt_pin)
    {
        assert(meta.evict_list_iter == m_evict_list.end());
        std::lock_guard evict_lock(m_evict_mutex);
        m_evict_list.emplace_front(&meta);
        meta.evict_list_iter = m_evict_list.begin();
    }
    meta.unlock();
}

void BufferManager::shard_t::mark_page_dirty(page_no_t page_no)
{
    m_lock.read_lock();
    auto it = m_metas.find(page_no);
    assert(it != m_metas.end());
    auto &meta = *it->second;
    m_lock.read_unlock();

    meta.lock();
    meta.dirty = true;
    int32_t old_size = meta.size;
    meta.size = meta.page->byte_size();
    m_loaded_size += -old_size + static_cast<int32_t>(meta.size);
    meta.unlock();
}

void BufferManager::shard_t::clear_cache()
{
    m_lock.write_lock();
    m_evict_mutex.lock();
    
    for(auto it = m_metas.begin(); it != m_metas.end();)
    {
        auto &meta = *it->second;
        meta.lock();
        const auto cnt_pin = meta.cnt_pin;
        meta.unlock();
        if(cnt_pin)
        {
            ++it;
        }
        else
        {
            m_evict_list.erase(meta.evict_list_iter);
            it = unload_page(it->first);
        }
    }

    if(m_loaded_size)
    {
        log_info("after clear_cache: " + std::to_string(m_metas.size()) + " cached pages (" +
                 std::to_string(m_loaded_size) + " bytes) remaining");
    }

    m_evict_mutex.unlock();
    m_lock.write_unlock();
}

void BufferManager::shard_t::flush_all_pages()
{
    m_lock.read_lock();
    for(auto &it : m_metas)
    {
        auto &meta = *it.second;
        meta.lock();
        flush_page(meta);
        meta.unlock();
    }
    m_lock.read_unlock();
}

void BufferManager::shard_t::discard_cache(internal_page_meta_t *meta)
{
    meta->lock();
    assert(!meta->cnt_pin);
    m_loaded_size -= meta->size;
    meta->unlock();

    delete meta->page;
    delete meta;
}

void BufferManager::shard_t::discard_cache(page_no_t page_no)
{
    m_lock.write_lock();
    auto it = m_metas.find(page_no);
    if(it == m_metas.end())
    {
        m_lock.write_unlock();
        return;
    }

    m_evict_mutex.lock();
    m_evict_list.erase(it->second->evict_list_iter);
    m_evict_mutex.unlock();

    discard_cache(it->second);
    m_metas.erase(it);
    m_lock.write_unlock();
}

void BufferManager::shard_t::discard_all_cache()
{
    m_lock.write_lock();
    m_evict_mutex.lock();
    for(auto it = m_metas.begin(); it != m_metas.end();)
    {
        discard_cache(it->second);
        it = m_metas.erase(it);
    }
    m_evict_mutex.unlock();
    m_lock.write_unlock();
}

void BufferManager::shard_t::flush_page(internal_page_meta_t &meta)
{
    if(meta.dirty)
    {
        bitstream bstream = meta.page->serialize();
        m_buffer.write_to_disk(meta.page_no, bstream);
        meta.dirty = false;
    }
}

void BufferManager::shard_t::flush_page(page_no_t page_no)
{
    m_lock.read_lock();
    auto it = m_metas.find(page_no);
    assert(it != m_metas.end());
    auto &meta = *it->second;
    m_lock.read_unlock();

    meta.lock();
    flush_page(meta);
    meta.unlock();
}

BufferManager::metas_map_t::iterator BufferManager::shard_t::unload_page(page_no_t page_no)
{
    auto it = m_metas.find(page_no);
    assert(it != m_metas.end());
    auto meta = it->second;
    auto next = m_metas.erase(it);

    meta->lock();
    assert(!meta->cnt_pin);
    flush_page(*meta);
    m_loaded_size -= meta->size;
    meta->unlock();

    delete meta->page;
    delete meta;
    return next;
}

void BufferManager::shard_t::check_evict()
{
#if defined(FAKE_ENCLAVE) && !defined(ALWAYS_PAGE)
    // no paging for benchmarking purposes
#else
    if(m_loaded_size < m_buffer_size)
    {
        return;
    }

    // evict more pages
    const auto expected = static_cast<size_t>(0.8 * m_buffer_size);
    size_t evict_cnt = 0, evict_size = 0;
    std::list<internal_page_meta_t *>::iterator it;
    internal_page_meta_t *meta;
    while(m_loaded_size > expected)
    {
        m_lock.read_to_write_lock();
        m_evict_mutex.lock();
        if(m_evict_list.empty())
        {
            m_evict_mutex.unlock();
            m_lock.write_to_read_lock();
            break;
        }

        it = std::prev(m_evict_list.end());
        meta = *it;
        assert(meta->evict_list_iter == it);
        ++evict_cnt;
        evict_size += meta->size;
        m_evict_list.erase(it);
        unload_page(meta->page_no);

        m_evict_mutex.unlock();
        m_lock.write_to_read_lock();
    }
#endif
}

BufferManager::BufferManager(EncryptedIO *encrypted_io, const std::string &file_prefix, size_t buffer_size)
: m_encrypted_io(encrypted_io), m_file_prefix(file_prefix), m_buffer_size(buffer_size), m_next_page_no(1)
{
    for(size_t i = 0; i < NUM_SHARDS; ++i)
    {
        m_shards[i] = new shard_t(*this, i, m_buffer_size / NUM_SHARDS);
    }
}

BufferManager::~BufferManager()
{
    for(auto &shard : m_shards)
    {
        delete shard;
    }
}

void BufferManager::mark_page_dirty(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->mark_page_dirty(page_no);
}

void BufferManager::flush_page(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->flush_page(page_no);
}

void BufferManager::unpin_page(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->unpin_page(page_no);
}

void BufferManager::clear_cache()
{
    for(auto &shard : m_shards)
    {
        shard->clear_cache();
    }
}

void BufferManager::discard_cache(page_no_t page_no)
{
    auto shard = m_shards[page_no % NUM_SHARDS];
    shard->discard_cache(page_no);
}

void BufferManager::discard_all_cache()
{
    for(auto &shard : m_shards)
    {
        shard->discard_all_cache();
    }
}

void BufferManager::flush_all_pages()
{
    for(auto &shard : m_shards)
    {
        shard->flush_all_pages();
    }
}

std::string BufferManager::page_filename(page_no_t page_no) const
{
    return m_file_prefix + "_page_" + std::to_string(page_no);
}

bitstream BufferManager::read_from_disk(page_no_t page_no)
{
    const std::string filename = page_filename(page_no);
    bitstream bstream;
    bool ok = m_encrypted_io->read_from_disk(filename, bstream);
    if(!ok)
    {
        log_error("Failed to read_from_disk: " + filename);
        abort();
    }
    return bstream;
}

void BufferManager::write_to_disk(page_no_t page_no, const bitstream &data)
{
    const std::string filename = page_filename(page_no);
    bool ok = m_encrypted_io->write_to_disk(filename, data);
    if(!ok)
    {
        log_error("Failed to write_to_disk: " + filename);
        abort();
    }
}

void BufferManager::set_encrypted_io(EncryptedIO *encrypted_io) { m_encrypted_io = encrypted_io; }

} // namespace trusted
} // namespace credb
