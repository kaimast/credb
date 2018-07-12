#pragma once

#include "HashMapNode.h"
#include <unordered_set>

namespace credb
{
namespace trusted
{

template<typename KeyType, typename ValueType>
class MultiMapNode : public HashMapNode<KeyType, ValueType>
{
public:
    using header_t = typename HashMapNode<KeyType, ValueType>::header_t;

    using HashMapNode<KeyType, ValueType>::HashMapNode;

    bool remove(const KeyType &key, const ValueType &value)
    {
        auto &data = this->m_data;
            
        auto old_pos = data.pos();

        bitstream view;
        view.assign(data.data(), data.size(), true);
        view.move_by(sizeof(header_t));

        while(!view.at_end())
        {
            auto pos = view.pos();

            KeyType k;
            ValueType v;

            view >> k >> v;

            auto end = view.pos();

            if(key == k && value == v)
            {
                data.move_to(pos);
                auto size = end - pos;
                data.remove_space(size);
                data.move_to(old_pos - size);

                auto &h = this->header();
                h.size -= 1;
                h.version.increment();

                this->mark_page_dirty();
                this->flush_page();
                return true;
            }
        }

        data.move_to(old_pos);
        return false;
    }

    size_t clear()
    {
        auto &data = this->m_data;
        auto &h = this->header();
        auto count = h.size;

        auto end = data.pos();
        data.move_to(sizeof(h));
        data.remove_space(end - data.pos());

        h.size = 0;

        this->mark_page_dirty();
        this->flush_page();
        return count;
    }

    void find_union(const KeyType &key, std::unordered_set<ValueType> &out)
    {
        auto &data = this->m_data;

        bitstream view;
        view.assign(data.data(), data.size(), true);

        view.move_by(sizeof(header_t));

        while(!view.at_end())
        {
            KeyType k;
            ValueType v;

            view >> k >> v;

            if(key == k)
            {
                out.insert(v);
            }
        }
    }

    bool insert(const KeyType &key, const ValueType &value)
    {
        if(this->byte_size() >= HashMapNode<KeyType, ValueType>::MAX_NODE_SIZE)
        {
            return false;
        }
        
        auto &data = this->m_data;
        data << key << value;

        auto &h = this->header();
        h.size += 1;
        h.version.increment();

        this->mark_page_dirty();
        this->flush_page();
        return true;
    }

    size_t estimate_value_count(const KeyType &key) const
    {
        // this is not an estimation, it's the correct number
        auto &data = this->m_data;
        size_t count = 0;

        bitstream view;
        view.assign(data.data(), data.size(), true);

        view.move_by(sizeof(header_t));

        while(!view.at_end())
        {
            KeyType k;
            ValueType v;

            view >> k >> v;

            if(key == k)
            {
                count += 1;
            }
        }

        return count;
    }
};

}
}
