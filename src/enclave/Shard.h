#pragma once

#include <bitstream.h>
#include <unordered_map>

#include "util/RWLockable.h"
#include "PageHandle.h"
#include "Block.h"
#include "ObjectEventHandle.h"

namespace credb
{
namespace trusted
{

class BufferManager;

using LedgerBlock = Block<ObjectEventHandle>;

class Shard : public RWLockable
{
public:
    explicit Shard(BufferManager &buffer);
    Shard(const Shard &other) = delete;

    shard_id_t identifier() const
    {
        return m_identifier;
    }
    
    page_no_t pending_block_id() const
    {
        return m_pending_block_id;
    }

    /**
     * @brief get a specific block
     * @note you must already hold the lock to this shard before calling this function
     * @param The type of lock *already held* by the calling procedure
     *
     * TODO replace LockType with a LockHandle
     */
    PageHandle<LedgerBlock> get_block(block_id_t block, LockType lock_type);

    PageHandle<LedgerBlock> generate_block();

    /// For downstream
    void set_pending_block(page_no_t id, block_index_t num_events);
    void discard_pending_block();
    void discard_cached_block(page_no_t page_no);

    /// For debug purposes
    /// TODO: remove these?
    void unload_everything();
    void dump_metadata(bitstream &output);
    void load_metadata(bitstream &input);

    PageHandle<LedgerBlock> get_pending_block(LockType lock_type);

private:
    BufferManager &m_buffer;
    shard_id_t m_identifier;

    //Needed for downstream
    page_no_t m_pending_block_id;
    block_index_t m_num_pending_events;

    PageHandle<LedgerBlock> m_pending_block;
};

inline PageHandle<LedgerBlock> Shard::get_pending_block(LockType lock_type)
{
    // get block might release the lock so we need to re-check after returning the block
    while(true)
    {
        auto pid = pending_block_id();
        auto block = get_block(pid, lock_type);
        
        if(pid == pending_block_id())
        {
            return block;
        }
        // else retry
    }
}
    
inline void Shard::set_pending_block(page_no_t id, block_index_t num_events)
{
    m_pending_block_id = id;
    m_num_pending_events = num_events;
}

} // namespace trusted
} // namespace credb
