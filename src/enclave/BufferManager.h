/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <atomic>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <condition_variable>
#include <unordered_map>

#include "util/RWLockable.h"
#include "EncryptedIO.h"
#include "Page.h"
#include "PageHandle.h"
#include "logging.h"

namespace credb::trusted
{

class BufferManager
{
    static constexpr size_t NUM_SHARDS = 32;

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
    
        /**
         * @brief Write page to disk, if dirty
         *
         * @note Before calling: no lock requirement
         */
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
        template <class T, class... Args> PageHandle<T> new_page(page_no_t page_no, Args &&... args);

        // Before calling: RLock shard_t
        template <class T> PageHandle<T> get_page_internal(page_no_t page_no, bool load);

        template<typename T> 
        void reload_page(page_no_t page_no);

        // Before calling: Lock internal_page_meta_t
        void pin_page(internal_page_meta_t &meta);

        // Before calling: no lock requirement
        void unpin_page(page_no_t page_no);

        // Before calling: Lock internal_page_meta_t
        void flush_page_internal(internal_page_meta_t &meta);

        // Before calling: WLock shard_t
        metas_map_t::iterator unload_page(page_no_t page_no);

        // Before calling: WLock shard_t
        void discard_cache(internal_page_meta_t *meta);

        /**
         * Evict pages, if needed
         *
         * Before calling: WLock shard_t
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

        std::mutex m_reload_mutex;
    };

public:
    BufferManager(EncryptedIO *encrypted_io, std::string file_prefix, size_t buffer_size);
    ~BufferManager();

    /**
     * Mark page as "dirty", i.e. it's content has changed
     * Before calling: no lock requirement
     */
    void mark_page_dirty(page_no_t page_no);

    template<typename T> 
    void reload_page(page_no_t page_no);

    /**
     * Write page to disk, if dirty
     *
     * @note before calling: no lock requirement
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

    EncryptedIO& get_encrypted_io() { return *m_encrypted_io; }

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

} // namespace credb::trusted

#include "BufferManager.inl"
#include "Page.inl"
