/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <bitstream.h>
#include <json/Document.h>

#include "util/RWLockable.h"
#include "credb/defines.h"
#include "credb/event_id.h"
#include "credb/event_id.h"
#include "credb/defines.h"

#include "Page.h"

namespace credb::trusted
{

/// smaller blocks are easier to copy in and out of the enclave
constexpr size_t MIN_BLOCK_SIZE = 5 * 1024; // 5kB

/// Determines the maximum entry/file size
using block_entry_size_t = uint32_t;

/**
 * All objects are stored in blocks
 * Blocks hold a single buffer that can easily be written to disk
 * All writes are append-only
 */
template<typename HandleType>
class Block : public Page
{
private:
    bitstream m_data;
    block_index_t m_file_pos;

    struct header_t
    {
        bool sealed;
        block_index_t num_files;
    };

    header_t& header()
    {
        return *reinterpret_cast<header_t*>(m_data.data());
    }

    const header_t& header() const
    {
        return *reinterpret_cast<const header_t*>(m_data.data());
    }

    size_t index_size() const;

    const block_entry_size_t* index() const
    {
        return reinterpret_cast<const block_entry_size_t*>(m_data.data()+sizeof(header_t));
    }

    block_entry_size_t* index()
    {
        return reinterpret_cast<block_entry_size_t*>(m_data.data()+sizeof(header_t));
    }

public:
    Block(BufferManager &buffer, page_no_t page_no, bool init);
    Block(BufferManager &buffer, page_no_t page_no, bitstream &bstream);
    Block(const Block &other) = delete;

    bitstream serialize() const override;
    
    /**
     * Get the amount of memory that is occupied by this datastructure
     *
     * @note this might contain memory that has been reserved but not written to yet
     */
    size_t byte_size() const override;

    /**
     * Get the number of bytes that are actual stored data
     */
    size_t get_data_size() const { return m_data.size(); }

    bool is_pending() const;
    
    block_index_t insert(json::Document &event);
    
    HandleType get(block_index_t pos) const;
    
    block_id_t identifier() const;
    void seal();
    void unseal(); // for debug purpose

    /**
     * How many entries are stored in this block? 
     */
    block_index_t num_entries() const
    {
        return m_file_pos+1;
    }
};


} // namespace credb::trusted

#include "Block.inl"
