#pragma once

#include <array>
#include "version_number.h"
#include "BufferManager.h"
#include "RWLockable.h"
#include "credb/event_id.h"
#include "credb/defines.h"
#include "util/defines.h"
#include "logging.h"
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
            shard.read_lock();
        }

        out << m_buckets;

        for(auto &shard: m_shards)
        {
            shard.read_unlock();
        }
    }

    void load_root(bitstream &in)
    {
        for(auto &shard : m_shards)
        {
            shard.write_lock();
        }

        in >> m_buckets;

        for(auto &shard : m_shards)
        {
            shard.write_unlock();
        }
    }

    void apply_changes(bitstream &changes)
    {
        bucketid_t bid;
        bucket_t new_val;
        changes >> bid >> new_val;

        auto &s = get_shard(bid);
        WriteLock lock(s);

        auto &bucket = m_buckets[bid];

        if(new_val.version < bucket.version)
        {
            // outdated update
            log_debug("received outdated updated");
            return;
        }

        // Discard all cached pages of that bucket
        auto current = m_buffer.get_page_if_cached<node_type>(bucket.page_no);

        std::vector<page_no_t> nodes;
        if(current)
        {
            nodes.push_back(current->page_no());
        }

        while(current)
        {
            auto succ = current->successor();
            nodes.push_back(current->page_no());
            current = m_buffer.get_page_if_cached<node_type>(succ);
        }

        for(auto node: nodes)
        {
            m_buffer.discard_cache(node);
        }

        bucket = new_val;

        m_bucket_cond.notify_all();
    }

protected:
    struct bucket_t
    {
        page_no_t page_no;
        version_number version;
    };

    PageHandle<node_type> get_successor(bucketid_t bid, PageHandle<node_type> &prev, const std::vector<page_no_t> &parents, bool create, RWHandle &shard_lock, bool modify = false)
    {
        auto succ = prev->successor();

        if(create || modify)
        {
            prev->increment_version_no();
        }

        auto node = get_node_internal(bid, succ, parents, create, shard_lock);

        if(create || modify)
        {
            prev->set_successor(node->page_no());
            prev->increment_successor_version();
        }

        return node;
    } 

    PageHandle<node_type> get_node(const bucketid_t bid, bool create, RWHandle &shard_lock, bool modify = false)
    {
        auto &bucket = m_buckets[bid];
        std::vector<page_no_t> parents;

        auto node = get_node_internal(bid, bucket.page_no, parents, create, shard_lock);

        if(create || modify)
        {
            bucket.page_no = node->page_no();
            bucket.version.increment();
        }
        
        return node;
    }

    bucketid_t to_bucket(KeyType key) const
    {
        return static_cast<bucketid_t>(hash<KeyType>(key) % NUM_BUCKETS);
    }

    bucket_t& get_bucket(bucketid_t id) 
    {
        return m_buckets[id];
    }

    RWLockable& get_shard(bucketid_t id) 
    {
        return m_shards[id % NUM_SHARDS];
    }


    std::atomic<size_t> m_size;

    AbstractMap(BufferManager &buffer, const std::string &name)
        : m_size(0), m_buffer(buffer)
    {
        (void)name;
        m_buckets.fill(bucket_t{INVALID_PAGE_NO, 0});
    }

private:
    PageHandle<node_type> get_node_internal(const bucketid_t bid, const page_no_t page_no, const std::vector<page_no_t>& parents, bool create, RWHandle &shard_lock)
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

        while(true)
        {
            auto node = m_buffer.get_page<node_type>(page_no);

            version_number expected_version;

            if(parents.empty())
            {
                expected_version = m_buckets[bid].version;
            }
            else
            {
                auto pid = *parents.rbegin();
                auto pparents = parents;
                pparents.pop_back();

                auto parent = get_node_internal(bid, pid, pparents, create, shard_lock);

                expected_version = parent->successor_version();
            }   

            if(node->version_no() != expected_version)
            {
                log_debug(std::to_string(node->version_no()) + " != " + std::to_string(expected_version));

                if(m_buffer.get_encrypted_io().is_remote())
                {
                    // The remote party might be in the process of updating it
                    // Notifications are always sent after updating the files on disk, so it safe to wait here

                    m_bucket_cond.wait(shard_lock);
                    node.clear();
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
            else
            {
                return node;
            }
        }
    }

    BufferManager &m_buffer;

   
    std::condition_variable_any m_bucket_cond;
    std::array<bucket_t, NUM_BUCKETS> m_buckets;
    std::array<RWLockable, NUM_SHARDS> m_shards;
};

}
}
