#pragma once

#include "util/defines.h"
#include <list>
#include <string>
#include <unordered_map>


namespace credb
{
namespace trusted
{


class EvictionAlgorithm
{
public:
    virtual ~EvictionAlgorithm() = default;

    virtual void touch(page_no_t page_no) = 0;
    virtual void remove(page_no_t page_no) = 0;
    virtual page_no_t evict() = 0;
};


class LruEviction : public EvictionAlgorithm
{
public:
    void touch(page_no_t page_no) override
    {
        remove(page_no);
        m_list.emplace_front(page_no);
        m_map.emplace(page_no, m_list.begin());
    }

    void remove(page_no_t page_no) override
    {
        auto it_map = m_map.find(page_no);
        if(it_map != m_map.end())
        {
            const auto &it_list = it_map->second;
            m_list.erase(it_list);
            m_map.erase(it_map);
        }
    }

    page_no_t evict() override
    {
        if(m_map.empty())
            return INVALID_PAGE_NO;
        auto it_list = m_list.end();
        --it_list;
        auto page_no = *it_list;
        m_map.erase(page_no);
        m_list.erase(it_list);
        return page_no;
    }

private:
    std::list<page_no_t> m_list;
    std::unordered_map<page_no_t, std::list<page_no_t>::iterator> m_map;
};


} // namespace trusted
} // namespace credb
