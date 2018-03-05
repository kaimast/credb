#pragma once

#include "Page.h"
#include "BufferManager.h"
#include "version_number.h"

namespace credb
{
namespace trusted
{

template<typename KeyType, typename ValueType>
class HashMapNode : public Page
{
public:
    static constexpr size_t MAX_NODE_SIZE = 1024;

    HashMapNode(BufferManager &buffer, page_no_t page_no)
        : Page(buffer, page_no)
    {
        header_t header = {0, INVALID_PAGE_NO, 0, 0};
        m_data << header;
    }

    HashMapNode(BufferManager &buffer, page_no_t page_no, bitstream &bstream)
        : Page(buffer, page_no)
    {
        uint8_t *buf;
        uint32_t len;
        bstream.detach(buf, len);
        m_data.assign(buf, len, false);
        m_data.move_to(m_data.size());
    }

    bitstream serialize() const override
    {
        bitstream bstream;
        bstream.assign(m_data.data(), m_data.size(), true);
        return bstream;
    }

    size_t byte_size() const override
    {
        return m_data.size() + sizeof(*this);
    }

    bool get(const KeyType& key, ValueType &value_out)
    {
        bitstream view;
        view.assign(m_data.data(), m_data.size(), true);

        view.move_by(sizeof(header_t));

        while(!view.at_end())
        {
            KeyType k;
            ValueType v;

            view >> k >> v;

            if(key == k)
            {
                value_out = v;
                return true;
            }
        }
        
        return false;
    }

    /**
     * Insert or update an entry
     *
     * @return True if successfully insert, False if not enough space
     */
    bool insert(const KeyType &key, const ValueType &value)
    {
        bitstream view;
        view.assign(m_data.data(), m_data.size(), true);

        view.move_by(sizeof(header_t));

        bool updated = false;

        while(!view.at_end())
        {
            KeyType k;
            view >> k;

            if(key == k)
            {
                view << value;

                mark_page_dirty();
                updated = true;
            }
            else
            {
                ValueType v;
                view >> v;
            }
        }
     
        if(updated || byte_size() < MAX_NODE_SIZE)
        {
            if(!updated)
            {
                m_data << key << value;
            }

            bitstream sview;
            sview.assign(m_data.data(), m_data.size(), true);

            header_t header;
            sview >> header;

            if(!updated)
            {
                header.size += 1;
            }
        
            header.version.increment();

            sview.move_to(0);
            sview << header;

            mark_page_dirty();
            return true;
        }
        else
        {
            return false;
        }
    }

    version_number version_no() const
    {
        return header().version;
    }

    /**
     * Increment the version no and successor version in the header
     * Only needed for iterator
     */
    void increment_version_no()
    {
        auto &h = header();
        h.version.increment();

        mark_page_dirty();
    }

    void increment_successor_version()
    {
        auto &h = header();
        h.successor_version.increment();

        mark_page_dirty();
    }

    void set_successor(page_no_t succ)
    {
        auto &h = header();
        h.successor = succ;

        mark_page_dirty();
    }


    /**
     *  Get the number of elements in this node
     *  @note this will return the number excluding successor
     */
    size_t size() const
    {
        return header().size;
    }

    bool empty() const
    {
        return size() == 0;
    }
    
    /**
     * Get entry at pos
     *
     * @param pos
     *      The position to query. Must be smaller than size()
     */
    std::pair<KeyType, ValueType> get(size_t pos) const
    {
        bitstream view;
        view.assign(m_data.data(), m_data.size(), true);
        view.move_by(sizeof(header_t));

        size_t cpos = 0;

        while(!view.at_end())
        {
            KeyType k;
            ValueType v;

            view >> k >> v;

            if(cpos == pos)
            {
                return {k, v};
            }

            cpos += 1;
        }
        
        throw std::runtime_error("HashMapNode::get falied: Out of bounds!");
    }

    /**
     * Does this node hold a specified entry?
     */
    bool has_entry(const KeyType &key, const ValueType &value) const
    {
        bitstream view;
        view.assign(m_data.data(), m_data.size(), true);

        view.move_by(sizeof(header_t));

        while(!view.at_end())
        {
            KeyType k;
            ValueType v;

            view >> k >> v;

            if(key == k && value == v)
            {
                return true;
            }
        }
        
        return false;
    }

    page_no_t successor() const
    {
        return header().successor;
    }

    version_number successor_version() const
    {
        return header().successor_version;
    }

protected:
    struct header_t
    {
        version_number version;
        page_no_t successor;
        version_number successor_version;
        size_t size;
    };

    const header_t& header() const
    {
        return *reinterpret_cast<const header_t*>(m_data.data());
    }

    header_t& header()
    {
        return *reinterpret_cast<header_t*>(m_data.data());
    }

    bitstream m_data;
};

}
}
