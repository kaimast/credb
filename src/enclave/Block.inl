
namespace credb
{
namespace trusted
{

template<typename HandleType>
Block<HandleType>::Block(BufferManager &buffer, page_no_t page_no, bool init)
: Page(buffer, page_no), m_file_pos(0)
{
    if(!init)
    {
        return;
    }

    // Avoid unnecessary allocs
    // Block size will be at least MIN_BLOCK_SIZE
    m_data.pre_alloc(MIN_BLOCK_SIZE);

    header_t h = { .sealed = false, .num_files = 50};
    m_data << h;
    memset(m_data.current(), 0, sizeof(block_entry_size_t) * h.num_files);
    m_data.move_by(h.num_files * sizeof(block_entry_size_t), true);
}

template<typename HandleType>
Block<HandleType>::Block(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
    : Page(buffer, page_no), m_file_pos(0)
{
    uint8_t *buf;
    uint32_t len;
    bstream.detach(buf, len);
    m_data.assign(buf, len, false);
    m_data.move_to(m_data.size());

    auto &h = header();
    auto idx = index(); 

    for(block_index_t i = 0; i < h.num_files; ++i)
    {
       if(idx[i] == 0)
       {
            break;
       }

       m_file_pos += 1;
    }
}

template<typename HandleType>
bitstream Block<HandleType>::serialize() const
{
    bitstream bstream;
    bstream.assign(m_data.data(), m_data.size(), true);
    return bstream;
}

template<typename HandleType>
bool Block<HandleType>::is_pending() const { return !header().sealed; }

template<typename HandleType>
size_t Block<HandleType>::index_size() const
{
    return header().num_files * sizeof(block_entry_size_t);
}

template<typename HandleType>
HandleType Block<HandleType>::get(block_index_t pos) const
{
    auto &h = this->header();

    if(pos >= h.num_files)
    {
        throw std::runtime_error("Block::get_event_failed: out of bounds");
    }

    auto idx = index();
    auto offset = idx[pos];
    auto idx_size = h.num_files * sizeof(block_entry_size_t);

    if(offset == 0)
    {
        throw std::runtime_error("Block::get_event failed: no such entry");
    }

    auto next = idx[pos + 1];
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
    return HandleType(std::move(view));
}

template<typename HandleType>
size_t Block<HandleType>::byte_size() const { return m_data.allocated_size() + sizeof(*this); }

template<typename HandleType>
block_id_t Block<HandleType>::identifier() const { return page_no(); }

template<typename HandleType>
void Block<HandleType>::seal()
{
    auto &h = header();

    if(h.sealed)
    {
        throw std::runtime_error("Block is already sealed");
    }

    h.sealed = true;
    mark_page_dirty();
}

template<typename HandleType>
void Block<HandleType>::unseal()
{
    auto &h = this->header();

    if(!h.sealed)
    {
        throw std::runtime_error("Block is not sealed");
    }

    h.sealed = false;
}

template<typename HandleType>
block_index_t Block<HandleType>::insert(json::Document &event)
{
    auto &h = header();
    auto idx = index();
    auto idx_size = index_size();
    auto pos = m_data.pos();

    if(h.sealed)
    {
        throw std::runtime_error("cannot insert. block is already sealed");
    }

    if(this->num_entries() >= h.num_files)
    {
        constexpr block_index_t FILE_INCREASE = 50;

        h.num_files += FILE_INCREASE;
        m_data.move_to(idx_size + sizeof(h));

        auto increase = FILE_INCREASE * sizeof(block_entry_size_t);
        m_data.make_space(increase);
        idx = index();

        memset(m_data.current(), 0, increase);

        pos = pos + increase;
        idx_size = idx_size + increase;

        m_data.move_to(pos);
    }

    idx[m_file_pos] = pos - idx_size;
    m_file_pos += 1;

    m_data.write_raw_data(event.data().data(), event.data().size());

    mark_page_dirty();
    return m_file_pos - 1;
}


} // namespace trusted
} // namespace credb
