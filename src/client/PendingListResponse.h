/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"
#include <json/json.h>

namespace credb
{

template <typename T> class PendingListResponse : public PendingMessage
{
private:
    std::vector<T> m_result;
    bool m_success = false;

public:
    PendingListResponse(operation_id_t id, ClientImpl &client) : PendingMessage(id, client) {}

    auto result() -> decltype(m_result) && { return std::move(m_result); }

    void parse(bitstream &msg) override
    {
        msg >> m_success;

        if(!m_success)
            return;

        uint32_t num_values = 0;
        msg >> num_values;

        for(uint32_t i = 0; i < num_values; ++i)
        {
            // FIXME use T
            m_result.emplace_back(json::Document(msg));
        }
    }

    bool success() const { return m_success; }
};
} // namespace credb
