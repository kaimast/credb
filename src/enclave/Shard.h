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

    shard_id_t identifier() const;
    block_id_t pending_block_id() const;
    PageHandle<Block> get_block(block_id_t block);
    PageHandle<Block> generate_block();

    void reload_pending_block(); // for downstream
    void discard_cached_block(page_no_t page_no); // for downstream
    void unload_everything(); // for debug purpose
    void dump_metadata(bitstream &output); // for debug purpose
    void load_metadata(bitstream &input); // for debug purpose

private:
    BufferManager &m_buffer;
    credb::Mutex  m_block_mutx;
    shard_id_t m_identifier;
    PageHandle<Block> m_pending_block;
};

} // namespace trusted
} // namespace credb
