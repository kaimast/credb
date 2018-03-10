#pragma once

#include "BufferManager.h"
#include "RWLockable.h"
#include "Block.h"
#include <bitstream.h>
#include <unordered_map>

namespace credb
{
namespace trusted
{

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

    PageHandle<Block> get_block(block_id_t block);

    PageHandle<Block> generate_block();

    /// For downstream
    void set_pending_block(page_no_t id)
    {
        if(id > m_pending_block_id)
        {
            m_pending_block_id = id;
            m_pending_block.clear();
        }
    }

    void discard_pending_block(); // for downstream
    void discard_cached_block(page_no_t page_no); // for downstream
    void unload_everything(); // for debug purpose
    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

private:
    BufferManager &m_buffer;
    credb::Mutex  m_block_mutx;
    shard_id_t m_identifier;

    page_no_t m_pending_block_id;
    PageHandle<Block> m_pending_block;
};

} // namespace trusted
} // namespace credb
