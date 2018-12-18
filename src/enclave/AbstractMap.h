/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <array>

#include "logging.h"
#include "version_number.h"
#include "BufferManager.h"

#include "util/RWLockable.h"
#include "credb/event_id.h"
#include "credb/defines.h"
#include "util/defines.h"
#include "util/hash.h"

namespace credb
{
namespace trusted
{

/**
 * Shared code between MultiMap and HashMap
 */
template<typename node_type, typename KeyType>
class AbstractMap
{
public:
    static constexpr size_t NUM_BUCKETS = 8192;
    static constexpr size_t NUM_SHARDS = 64;

    using bucketid_t = uint16_t;
    
    size_t size() const
    {
        return m_size;
    }

    void serialize_root(bitstream &out)
    {
        for(auto &shard: m_shards)
        {
            shard.mutex.read_lock();
        }

        out << m_buckets;

        for(auto &shard: m_shards)
        {
            shard.mutex.read_unlock();
        }
    }

    void load_root(bitstream &in)
    {
        for(auto &shard : m_shards)
        {
            shard.mutex.write_lock();
        }

        in >> m_buckets;

        for(auto &shard : m_shards)
        {
            shard.mutex.write_unlock();
        }
    }

    void apply_changes(bitstream &changes)
    {
        bucketid_t bid;
        bucket_t new_val;
        changes >> bid >> new_val;

        auto &s = get_shard(bid);
        WriteLock lock(s.mutex);

        auto &bucket = m_buckets[bid];

        if(new_val.version < bucket.version)
        {
            // outdated update
            log_debug("received outdated updated");
            return;
        }

        bucket = new_val;
        s.condition_var.notify_all();
    }

protected:
    struct bucket_t
    {
        page_no_t page_no;
        version_number version;
    };

    struct shard_t
    {
        RWLockable mutex;
        std::condition_variable_any condition_var;
    };

    /**
     * Get the successor a specific node
     *
     * If executed as downstream, this might wait on upstream to send index updates
     */
    PageHandle<node_type> get_successor(bucketid_t bid, PageHandle<node_type> &prev, bool create, RWHandle &shard_lock, bool modify = false)
    {
        auto succ = prev->successor();
        const bool will_modify = create || (modify && succ != INVALID_PAGE_NO);

        if(will_modify)
        {
            prev->increment_version_no();
        }

        auto node = get_node_internal(bid, succ, prev->successor_version(), create, shard_lock);

        if(will_modify)
        {
            if(!node)
            {
                throw std::runtime_error("Invalid state!");
            }

            prev->set_successor(node->page_no());
            prev->increment_successor_version();
            prev->flush_page();
        }

        return node;
    } 

    PageHandle<node_type> get_node(const bucketid_t bid, bool create, RWHandle &shard_lock, bool modify = false)
    {
        auto &bucket = m_buckets[bid];
        //        std::vector<page_no_t> parents;
        //std::vector<page_no_t>::const_reverse_iterator pit = parents.rbegin();

        const auto page_no = bucket.page_no;
        const bool will_modify = create || (modify && page_no != INVALID_PAGE_NO);
        
        auto node = get_node_internal(bid, bucket.page_no, bucket.version, create, shard_lock);

        if(will_modify)
        {
            bucket.page_no = node->page_no();
            bucket.version.increment();

            node->flush_page();
        }

        return node;
    }

    /**
     * Find a key's corresponding bucket id
     */
    bucketid_t to_bucket(KeyType key) const
    {
        return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
    }

    /**
     * Retrieve the bucket datastructure from the bucket id
     */
    bucket_t& get_bucket(bucketid_t id) 
    {
        return m_buckets[id];
    }

    shard_t& get_shard(bucketid_t id) 
    {
        return m_shards[id % NUM_SHARDS];
    }

    std::atomic<size_t> m_size;

    AbstractMap(BufferManager &buffer, const std::string &name)
        : m_size(0), m_buffer(buffer)
    {
        (void)name;
        bucket_t default_bucket = { .page_no = INVALID_PAGE_NO, .version = 0};
        m_buckets.fill(default_bucket);
    }

private:
    PageHandle<node_type> get_node_internal(bucketid_t bid, const page_no_t page_no, const version_number &expected_version, bool create, RWHandle &shard_lock)
    {
        if(page_no == INVALID_PAGE_NO)
        {
            if(create)
            {
                return m_buffer.new_page<node_type>();
            }
            else
            {
                return PageHandle<node_type>();
            }
        }

        if(m_buffer.get_encrypted_io().is_remote())
        {
            shard_lock.unlock();
            
            while(true)
            {
                auto node = m_buffer.get_page<node_type>(page_no);

                if(node->version_no() < expected_version)
                {
                    m_buffer.reload_page<node_type>(page_no);
                }
                else if(node->version_no() == expected_version)
                {
                    shard_lock.lock();
                    return node;
                }
                else
                {
                    // The remote party might be in the process of updating it
                    // Notifications are always sent after updating the files on disk, so it safe to wait here
                    auto &s = get_shard(bid);
                    s.condition_var.wait(shard_lock);
                }
            }

            shard_lock.lock();
        }
        else
        {
            auto node = m_buffer.get_page<node_type>(page_no);

            if(!node)
            {
                throw std::runtime_error("Invalid state: No such node");
            }
            else if(node->version_no() == expected_version)
            {
                return node;
            }
            else
            {
                auto msg = "Staleness detected! HasMap node: " + std::to_string(page_no) +
                       "  Expected version: " + std::to_string(expected_version) +
                       "  Read: " + std::to_string(node->version_no());
              
                log_error(msg);
                throw StalenessDetectedException(msg);
            }
        }
    }

    BufferManager &m_buffer;

    std::array<bucket_t, NUM_BUCKETS> m_buckets;
    std::array<shard_t, NUM_SHARDS> m_shards;
};

}
}
