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
private:
    bitstream m_data;
    bool m_sealed;
    uint32_t m_file_pos;

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
    uint32_t index_size() const;
    event_index_t insert(json::Document &event);
    ObjectEventHandle get_event(event_index_t idx) const;
    uint32_t num_events() const;
    block_id_t identifier() const;
    void seal();
    void unseal(); // for debug purpose
};


} // namespace trusted
} // namespace credb
