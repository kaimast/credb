#pragma once

#include <unordered_map>

#include "PageHandle.h"
#include "Shard.h"

namespace credb
{
namespace trusted
{

class Ledger;

class would_block_exception
{
};

/**
 * This class is used to keep track of locks used for a specific operation
 * Once the class is destroyed it will release all the locks
 * Don't use Shard::get_block whenever you can avoid it and use LockHandle::get_block instead
 *
 * @note This class shall only be used by one thread
 */
class LockHandle
{
public:
    /**
     * Construct a new lock handle
     *
     * @param ledger
     *    The associated ledger object
     *
     * @param parent [optional]
     *    The parent lock handle.
     *    If given, all locks will be acquired through the parent
     *
     * @param nonblocking [optional]
     *    If set to true, the lock handle will throw would_block_exception instead of waiting for a lock
     */
    explicit LockHandle(Ledger &ledger, LockHandle *parent = nullptr, bool nonblocking = false);

    LockHandle(const LockHandle &other) = delete;

    /**
     * Move constructor
     */
    LockHandle(LockHandle &&other) noexcept;

    /**
     * Destructor
     *
     * @note this will release all held locks (same semantics as clear())
     */
    ~LockHandle();

    /**
     *  Release all locks held by this handle
     */
    void clear();

    /**
     * Get and acquire lock to a block
     */ 
    PageHandle<LedgerBlock> get_block(shard_id_t shard_no, block_id_t block, LockType lock_type);

    /**
     * Get and acquire lock to the pending (most recent) block of the shard
     */ 
    PageHandle<LedgerBlock> get_pending_block(shard_id_t shard_no, LockType lock_type);

    void release_block(shard_id_t shard_no, block_id_t block, LockType lock_type);

    /**
     * Acquire lock and return a handle to a shard
     */
    Shard &get_shard(shard_id_t shard_no, LockType lock_type, bool nonblocking = false);

    void release_shard(shard_id_t shard_no,LockType lock_type);

    bool has_parent() const;

    size_t num_locks() const;

    void set_blocking(bool val);

private:
    struct LockInfo
    {
        LockInfo(Shard &shard_, LockType type) : shard(shard_), read_count(0), write_count(0)
        {
            if(type == LockType::Read)
            {
                read_count = 1;
            }
            else
            {
                write_count = 1;
            }
        }

        LockType lock_type() const
        {
            if(write_count > 0)
            {
                return LockType::Write;
            }
            else
            {
                return LockType::Read;
            }
        }

        Shard &shard;
        uint16_t read_count, write_count;
    };

    Ledger &m_ledger;
    LockHandle *const m_parent;

    std::unordered_map<shard_id_t, LockInfo> m_locks;

    bool m_nonblocking;
};

inline PageHandle<LedgerBlock> LockHandle::get_pending_block(shard_id_t shard_no, LockType lock_type)
{
    auto &shard = get_shard(shard_no, lock_type);
    return shard.get_pending_block(lock_type);
}

inline bool LockHandle::has_parent() const
{
    return m_parent != nullptr;
}

inline size_t LockHandle::num_locks() const
{
    return m_locks.size();
}

inline void LockHandle::set_blocking(bool val)
{
    m_nonblocking = !val;
}

} // namespace trusted
} // namespace credb
