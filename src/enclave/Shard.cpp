#include "Shard.h"

namespace credb
{
namespace trusted
{

Shard::Shard(BufferManager &buffer) : m_buffer(buffer)
    {}

shard_id_t Shard::identifier() const { return m_identifier; }

block_id_t Shard::pending_block_id() const { return m_pending_block->page_no(); }

PageHandle<Block> Shard::get_block(block_id_t block)
{
    page_no_t page_no = block;
    return m_buffer.get_page<Block>(page_no);
}

PageHandle<Block> Shard::generate_block()
{
    m_pending_block = m_buffer.new_page<Block>(true);
    return m_buffer.get_page<Block>(m_pending_block->page_no());
}

void Shard::unload_everything() { m_pending_block.clear(); }

void Shard::dump_metadata(bitstream &output) { output << m_pending_block->page_no(); }

void Shard::load_metadata(bitstream &input)
{
    page_no_t page_no;
    input >> page_no;
    m_pending_block = m_buffer.get_page<Block>(page_no);
    m_pending_block->unseal();
}

void Shard::reload_pending_block()
{
    auto page_no = m_pending_block->page_no();
    m_pending_block.clear();
    m_buffer.discard_cache(page_no);
    m_pending_block = m_buffer.get_page<Block>(page_no);
    m_pending_block->unseal();
}

void Shard::discard_cached_block(page_no_t page_no)
{
    bool is_pending_block = m_pending_block->page_no() == page_no;
    if(is_pending_block)
    {
        m_pending_block.clear();
    }
    m_buffer.discard_cache(page_no);
    if(is_pending_block)
    {
        m_pending_block = m_buffer.get_page<Block>(page_no);
        m_pending_block->unseal();
    }
}


} // namespace trusted
} // namespace credb
