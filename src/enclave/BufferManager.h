#pragma once

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "EncryptedIO.h"
#include "RWLockable.h"
#include "Page.h"
#include "PageHandle.h"
#include "logging.h"

namespace credb
{
namespace trusted
{

class BufferManager
{
    static constexpr size_t NUM_SHARDS = 32;

    struct internal_page_meta_t : public credb::Mutex
    {
        page_no_t page_no;
        Page *page;
        size_t size;
        size_t cnt_pin;
        bool dirty;
        std::list<internal_page_meta_t *>::iterator evict_list_iter;

        internal_page_meta_t(page_no_t _page_no, Page *_page)
        : page_no(_page_no), page(_page), size(page->byte_size()), cnt_pin(0), dirty(false)
        {
        }
    };

    using metas_map_t = std::unordered_map<page_no_t, internal_page_meta_t *>;

    class shard_t
    {
    public:
        shard_t(BufferManager &buffer, size_t shard_id, size_t buffer_size)
        : m_buffer(buffer), m_shard_id(shard_id), m_buffer_size(buffer_size), m_loaded_size(0)
        {
        }

        ~shard_t();

        // Before calling: no lock requirement
        void mark_page_dirty(page_no_t page_no);

        // Before calling: no lock requirement
        void flush_page(page_no_t page_no);

        // Before calling: no lock requirement
        void flush_all_pages();

        // Before calling: no lock requirement
        void clear_cache();

        // Before calling: no lock requirement
        void discard_cache(page_no_t page_no);

        // Before calling: no lock requirement
        void discard_all_cache();

        // Before calling: no lock requirement
        template <class T> PageHandle<T> get_page_if_cached(page_no_t page_no)
        {
            m_lock.read_lock();
            auto page = get_page_internal<T>(page_no, false);
            m_lock.read_unlock();
            return page;
        }

        // Before calling: no lock requirement
        template <class T> PageHandle<T> get_page(page_no_t page_no)
        {
            m_lock.read_lock();
            auto page = get_page_internal<T>(page_no, true);
            m_lock.read_unlock();
            return page;
        }

        // Before calling: no lock requirement
        template <class T, class... Args> PageHandle<T> new_page(page_no_t page_no, Args &&... args)
        {
            m_lock.write_lock();

            auto page = new T(m_buffer, page_no, std::forward<Args>(args)...);
            auto meta = new internal_page_meta_t(page_no, page);
            m_loaded_size += meta->size;
            meta->evict_list_iter = m_evict_list.end();
            meta->dirty = true;
            m_metas[page_no] = meta;

            m_lock.write_to_read_lock();
            auto handle = get_page_internal<T>(page_no, true);
            m_lock.read_unlock();
            return handle;
        }

        // Before calling: RLock shard_t
        template <class T> PageHandle<T> get_page_internal(page_no_t page_no, bool load)
        {
            check_evict();

            auto it = m_metas.find(page_no);
            internal_page_meta_t *meta = nullptr;
            T *page = nullptr;

            if(it != m_metas.end())
            {
                meta = it->second;
                page = static_cast<T *>(meta->page);
                assert(page != nullptr);
            }
            else
            {
                if(!load)
                {
                    // not loaded
                    return PageHandle<T>();
                }

                m_lock.read_to_write_lock();
                bitstream bstream = m_buffer.read_from_disk(page_no);
                page = new T(m_buffer, page_no, bstream);
                meta = new internal_page_meta_t(page_no, page);
                meta->evict_list_iter = m_evict_list.end();

                // double check, in case of another thread has loaded it just now
                it = m_metas.find(page_no);
                if(it != m_metas.end())
                {
                    delete meta;
                    delete page;
                    meta = it->second;
                    page = static_cast<T *>(meta->page);
                    assert(page != nullptr);
                }
                else
                {
                    m_loaded_size += meta->size;
                    m_metas[page_no] = meta;
                }
                m_lock.write_to_read_lock();
            }
            meta->lock();
            pin_page(*meta);
            meta->unlock();

            return PageHandle<T>(*page);
        }

        // Before calling: Lock internal_page_meta_t
        void pin_page(internal_page_meta_t &meta);

        // Before calling: no lock requirement
        void unpin_page(page_no_t page_no);

        // Before calling: Lock internal_page_meta_t
        void flush_page(internal_page_meta_t &meta);

        // Before calling: WLock shard_t
        metas_map_t::iterator unload_page(page_no_t page_no);

        // Before calling: WLock shard_t
        void discard_cache(internal_page_meta_t *meta);

        /**
         * Evict pages, if needed
         *
         * Before calling: RLock shard_t
         */
        void check_evict();

        RWLockable m_lock;
        BufferManager &m_buffer;
        const size_t m_shard_id;
        const size_t m_buffer_size;
        metas_map_t m_metas; // metadata of loaded pages
        std::atomic<size_t> m_loaded_size;

        Mutex m_evict_mutex;
        std::condition_variable_any m_evict_condition;

        std::list<internal_page_meta_t *> m_evict_list;
    };

public:
    BufferManager(EncryptedIO *encrypted_io, const std::string &file_prefix, size_t buffer_size);
    ~BufferManager();

    /**
     * Mark page as "dirty", i.e. it's content has changed
     * Before calling: no lock requirement
     */
    void mark_page_dirty(page_no_t page_no);

    /**
     * Write page to disk
     * Before calling: no lock requirement
     */
    void flush_page(page_no_t page_no);

    // Before calling: no lock requirement
    void flush_all_pages();

    /**
     * Unload all pages
     * Before calling: no lock requirement
     */
    void clear_cache();

    // Before calling: no lock requirement
    void discard_cache(page_no_t page_no);

    // Before calling: no lock requirement
    void discard_all_cache();

    // Before calling: no lock requirement
    template <class T> PageHandle<T> get_page_if_cached(page_no_t page_no)
    {
        auto shard = m_shards[page_no % NUM_SHARDS];
        return shard->get_page_if_cached<T>(page_no);
    }

    // Before calling: no lock requirement
    template <class T> PageHandle<T> get_page(page_no_t page_no)
    {
        auto shard = m_shards[page_no % NUM_SHARDS];
        return shard->get_page<T>(page_no);
    }

    // Before calling: no lock requirement
    template <class T, class... Args> PageHandle<T> new_page(Args &&... args)
    {
        page_no_t page_no = m_next_page_no++;
        auto shard = m_shards[page_no % NUM_SHARDS];
        return shard->new_page<T, Args...>(page_no, std::forward<Args>(args)...);
    }

    // Before calling: no lock requirement
    void unpin_page(page_no_t page_no);

    // Before calling: no lock required
    std::string page_filename(page_no_t page_no) const;

    // Before calling: no lock required
    void set_encrypted_io(EncryptedIO *encrypted_io);

private:
    // Before calling: no lock required
    bitstream read_from_disk(page_no_t page_no);

    // Before calling: no lock required
    void write_to_disk(page_no_t page_no, const bitstream &data);

    EncryptedIO *m_encrypted_io;
    const std::string m_file_prefix;
    const size_t m_buffer_size;
    std::atomic<page_no_t> m_next_page_no;
    shard_t *m_shards[NUM_SHARDS];
};

inline void Page::flush_page() { m_buffer.flush_page(m_page_no); }

inline void Page::mark_page_dirty() { m_buffer.mark_page_dirty(m_page_no); }

} // namespace trusted
} // namespace credb
