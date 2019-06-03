/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Shard.h"
#include "BufferManager.h"

namespace credb::trusted
{

Shard::Shard(BufferManager &buffer) : m_buffer(buffer)
{
}

PageHandle<LedgerBlock> Shard::get_block(block_id_t block, LockType lock_type)
{
    PageHandle<LedgerBlock> hdl;
    page_no_t page_no = block;
    
    if(page_no == m_pending_block_id)
    {
        if(m_buffer.get_encrypted_io().is_remote())
        {
            this->unlock(lock_type);
            hdl = m_buffer.get_page<LedgerBlock>(page_no);

            // Make sure we have the most recent version
            while(hdl->num_entries() < m_num_pending_events)
            {
                m_buffer.reload_page<LedgerBlock>(page_no);
            }

            this->lock(lock_type);
        }
        else
        {
            // Can't be evicted so must be the most recent
            hdl = m_buffer.get_page<LedgerBlock>(page_no);
        }
    }
    else if(page_no < m_pending_block_id)
    {
        // Make sure we have the most recent version
        if(m_buffer.get_encrypted_io().is_remote())
        {
            this->unlock(lock_type);
            hdl = m_buffer.get_page<LedgerBlock>(page_no);

            while(hdl->is_pending())
            {
                m_buffer.reload_page<LedgerBlock>(page_no);
            }
            this->lock(lock_type);
        }
        else
        {
            hdl = m_buffer.get_page<LedgerBlock>(page_no);
            
            if(hdl->is_pending())
            {
                throw StalenessDetectedException("Old data block was loaded!");
            }
        }
    }
    else
    {
        // FIXME
        // this shouldn't be a problem in most cases but is super ugly
        log_warning("Getting more recent block?");
        hdl = m_buffer.get_page<LedgerBlock>(page_no);
    }

    return hdl;
}

PageHandle<LedgerBlock> Shard::generate_block()
{
    m_pending_block = m_buffer.new_page<LedgerBlock>(true);
    m_pending_block_id = m_pending_block->page_no();
    return m_buffer.get_page<LedgerBlock>(m_pending_block->page_no());
}

void Shard::unload_everything() { m_pending_block.clear(); }

void Shard::dump_metadata(bitstream &output) { output << m_pending_block->page_no(); }

void Shard::load_metadata(bitstream &input)
{
    page_no_t page_no;
    input >> page_no;
    m_pending_block = m_buffer.get_page<LedgerBlock>(page_no);

    if(!m_pending_block.is_valid())
    {
        log_fatal("Invalid state: no such meta data page");
    }

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


} // namespace credb::trusted
