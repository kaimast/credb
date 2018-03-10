#include "Shard.h"

namespace credb
{
namespace trusted
{

Shard::Shard(BufferManager &buffer) : m_buffer(buffer)
{
}

PageHandle<Block> Shard::get_block(block_id_t block)
{
    page_no_t page_no = block;
    auto hdl = m_buffer.get_page<Block>(page_no);

    if(page_no == m_pending_block_id)
    {
        if(m_buffer.get_encrypted_io().is_remote())
        {
            // Make sure we have the most recent version
            while(hdl->num_events() < m_num_pending_events)
            {
                m_buffer.reload_page<Block>(page_no);
            }
        }
    }
    else if(page_no < m_pending_block_id)
    {
        if(hdl->is_pending())
        {
            // Make sure we have the most recent version
            if(m_buffer.get_encrypted_io().is_remote())
            {
                m_buffer.reload_page<Block>(page_no);
            }
            
            if(hdl->is_pending())
            {
                throw StalenessDetectedException("Old data block was loaded!");
            }
        }
    }
    else
    {
// this is probably okay        throw std::runtime_error("Shard::get_block failed: invalid state");
    }

    return hdl;
}

PageHandle<Block> Shard::generate_block()
{
    m_pending_block = m_buffer.new_page<Block>(true);
    m_pending_block_id = m_pending_block->page_no();
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

void Shard::discard_pending_block()
{
    auto page_no = m_pending_block->page_no();
    m_pending_block.clear();
    m_buffer.discard_cache(page_no);
}

void Shard::discard_cached_block(page_no_t page_no)
{
    bool is_pending_block = pending_block_id() == page_no;
    if(is_pending_block)
    {
        m_pending_block.clear();
    }

    m_buffer.discard_cache(page_no);
}


} // namespace trusted
} // namespace credb
