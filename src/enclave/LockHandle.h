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

// This class is used to keep track of locks used for a specific operation
// Once the class is destroyed it will release all the locks
// Don't use Shard::get_block whenever you can avoid it and use LockHandle::get_block instead
class LockHandle
{
public:
    explicit LockHandle(Ledger &ledger, LockHandle *parent = nullptr);

    LockHandle(const LockHandle &other) = delete;

    LockHandle(LockHandle &&other) noexcept;

    ~LockHandle();

    void clear();

    PageHandle<Block> get_block(shard_id_t shard_no, block_id_t block, LockType lock_type);

    PageHandle<Block> get_pending_block(shard_id_t shard_no, LockType lock_type);

    void release_block(shard_id_t shard_no, block_id_t block, LockType lock_type);

    Shard &get_shard(shard_id_t shard_no, LockType lock_type);

    void release_shard(shard_id_t shard_no,LockType lock_type);

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
    LockHandle *m_parent;

    std::unordered_map<shard_id_t, LockInfo> m_locks;
};

} // namespace trusted
} // namespace credb
