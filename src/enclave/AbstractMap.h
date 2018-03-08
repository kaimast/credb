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

    PageHandle<node_type> get_successor(bucketid_t bid, PageHandle<node_type> &prev, const std::vector<page_no_t> &parents, bool create, RWHandle &shard_lock, bool modify = false)
    {
        auto succ = prev->successor();
        const bool will_modify = create || (modify && succ != INVALID_PAGE_NO);

        if(will_modify)
        {
            prev->increment_version_no();
        }

        auto node = get_node_internal(bid, succ, parents, create, shard_lock);

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
        std::vector<page_no_t> parents;

        const auto page_no = bucket.page_no;
        const bool will_modify = create || (modify && page_no != INVALID_PAGE_NO);

        auto node = get_node_internal(bid, page_no, parents, create, shard_lock);

        if(will_modify)
        {
            bucket.page_no = node->page_no();
            bucket.version.increment();

            node->flush_page();
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
            PageHandle<node_type> node;

            if(m_buffer.get_encrypted_io().is_remote())
            {
                // This might wait on upstream to update the data
                // So we have to unlock before we wait
                shard_lock.unlock();
                node = m_buffer.get_page<node_type>(page_no);
                shard_lock.lock(); 
            }
            else
            {
                node = m_buffer.get_page<node_type>(page_no);
            }

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

            if(!node)
            {
                throw std::runtime_error("Invalid state: No such node");
            }

            if(node->version_no() != expected_version)
            {
                log_debug(std::to_string(page_no) + ": " + std::to_string(node->version_no()) + " != " + std::to_string(expected_version));

                if(m_buffer.get_encrypted_io().is_remote())
                {
                    if(node->version_no() < expected_version)
                    {
                        // If we end up here the upstream probably didn't flush it's state to disk before pushing the index update
                        log_error("Invalid state: Index more recent than file");
                        abort();
                    }

                    // The remote party might be in the process of updating it
                    // Notifications are always sent after updating the files on disk, so it safe to wait here

                    node.clear();

                    auto &s = get_shard(bid);
                    s.condition_var.wait(shard_lock);
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

    std::array<bucket_t, NUM_BUCKETS> m_buckets;
    std::array<shard_t, NUM_SHARDS> m_shards;
};

}
}
