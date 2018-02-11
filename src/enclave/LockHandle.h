#pragma once

#include "Block.h"
#include "RWLockable.h"
#include "PageHandle.h"
#include <unordered_map>

namespace credb
{
namespace trusted
{

class Shard;
class Ledger;

/**
 * This class is used to keep track of locks used for a specific operation
 * Once the class is destroyed it will release all the locks
 * Don't use Shard::get_block whenever you can avoid it and use LockHandle::get_block instead
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
     */
    explicit LockHandle(Ledger &ledger, LockHandle *parent = nullptr);

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
    PageHandle<Block> get_block(shard_id_t shard_no, block_id_t block, LockType lock_type);

    PageHandle<Block> get_pending_block(shard_id_t shard_no, LockType lock_type);

    void release_block(shard_id_t shard_no, block_id_t block, LockType lock_type);

    Shard &get_shard(shard_id_t shard_no, LockType lock_type);

    void release_shard(shard_id_t shard_no,LockType lock_type);

    bool has_parent() const
    {
        return m_parent != nullptr;
    }

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
};

} // namespace trusted
} // namespace credb
