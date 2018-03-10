#pragma once

#include <bitstream.h>
#include <json/Document.h>

#include "RWLockable.h"
#include "Page.h"
#include "credb/defines.h"

namespace credb
{
namespace trusted
{

class BufferManager;
class ObjectEventHandle;

/**
 * All objects are stored in blocks
 * Blocks hold a single buffer that can easily be written to disk
 * All writes are append-only
 */
class Block : public Page
{
public:
    using int_type = uint32_t;

private:
    bitstream m_data;
    int_type m_file_pos;

    struct header_t
    {
        bool sealed;
        int_type num_files;
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

    const int_type* index() const
    {
        return reinterpret_cast<const int_type*>(m_data.data()+sizeof(header_t));
    }

    int_type* index()
    {
        return reinterpret_cast<int_type*>(m_data.data()+sizeof(header_t));
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
    event_index_t insert(json::Document &event);
    ObjectEventHandle get_event(event_index_t pos) const;
    block_id_t identifier() const;
    void seal();
    void unseal(); // for debug purpose

    /**
     * How many entries are stored in this block? 
     */
    int_type num_events() const
    {
        return m_file_pos+1;
    }

};


} // namespace trusted
} // namespace credb
