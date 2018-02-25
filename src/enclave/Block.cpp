#include "Block.h"
#include "Ledger.h"
#include "logging.h"
#include "BufferManager.h"

namespace credb
{
namespace trusted
{

Block::Block(BufferManager &buffer, page_no_t page_no, bool init)
: Page(buffer, page_no), m_sealed(false), m_file_pos(0)
{
    if(!init)
    {
        return;
    }

    // Avoid unneccesary allocs
    // Block size will be at least MIN_BLOCK_SIZE
    m_data.pre_alloc(MIN_BLOCK_SIZE);

    uint32_t n_files = 50;
    m_data << n_files;
    memset(m_data.current(), 0, sizeof(uint32_t) * n_files);
    m_data.move_by(n_files * sizeof(uint32_t), true);
}

Block::Block(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
: Page(buffer, page_no), m_sealed(false), m_file_pos(0)
{
    uint8_t *buf;
    uint32_t len;
    bstream.detach(buf, len);
    m_data.assign(buf, len, false);
    m_data.move_to(m_data.size());

    auto index = reinterpret_cast<const uint32_t *>(m_data.data());
    auto nfiles = index[0];

    for(uint32_t i = 1; i <= nfiles; ++i)
    {
        if(index[i] == 0)
        {
            break;
        }

        m_file_pos += 1;
    }

    seal();
}

bitstream Block::serialize() const
{
    bitstream bstream;
    bstream.assign(m_data.data(), m_data.size(), true);
    return bstream;
}

bool Block::is_pending() const { return !m_sealed; }

uint32_t Block::index_size() const
{
    auto idx = reinterpret_cast<const uint32_t *>(m_data.data());
    auto n_files = idx[0];
    return n_files * sizeof(uint32_t);
}

ObjectEventHandle Block::get_event(event_index_t idx) const
{
    auto index = reinterpret_cast<const uint32_t *>(m_data.data());
    if(idx >= index[0])
    {
        throw std::runtime_error("Block::get_event_failed: out of bounds");
    }

    auto offset = index[idx + 1];
    auto idx_size = index[0] * sizeof(uint32_t);

    if(offset == 0)
    {
        throw std::runtime_error("Block::get_event failed: no such entry");
    }

    auto next = index[idx + 2];
    if(next == 0)
    {
        next = m_data.pos();
    }
    else
    {
        next = next + idx_size;
    }

    offset = offset + idx_size;
    auto size = next - offset;

    json::Document view(m_data.data() + offset, size, json::DocumentMode::ReadOnly);
    return ObjectEventHandle(std::move(view));
}

uint32_t Block::num_events() const { return m_file_pos; }

size_t Block::byte_size() const { return m_data.allocated_size() + sizeof(*this); }

block_id_t Block::identifier() const { return page_no(); }

void Block::seal()
{
    if(m_sealed)
    {
        throw std::runtime_error("Block is already sealed");
    }

    m_sealed = true;
}

void Block::unseal()
{
    if(!m_sealed)
    {
        throw std::runtime_error("Block is not sealed");
    }

    m_sealed = false;
    m_data.move_to(m_data.size());
}

event_index_t Block::insert(json::Document &event)
{
    if(m_sealed)
    {
        throw std::runtime_error("cannot insert. block is already sealed");
    }

    auto index = reinterpret_cast<uint32_t *>(m_data.data());
    auto &n_files = index[0];

    auto idx_size = index_size();
    auto pos = m_data.pos();

    if(m_file_pos + 1 >= n_files)
    {
        constexpr uint32_t FILE_INCREASE = 50;

        n_files += FILE_INCREASE;
        m_data.move_to(idx_size);

        auto increase = FILE_INCREASE * sizeof(uint32_t);
        m_data.make_space(increase);
        index = reinterpret_cast<uint32_t *>(m_data.data());

        memset(m_data.current(), 0, increase);

        pos = pos + increase;
        idx_size = idx_size + increase;

        m_data.move_to(pos);
    }

    index[m_file_pos + 1] = pos - idx_size;
    m_file_pos += 1;

    m_data.write_raw_data(event.data().data(), event.data().size());

    mark_page_dirty();
    return m_file_pos - 1;
}


} // namespace trusted
} // namespace credb
