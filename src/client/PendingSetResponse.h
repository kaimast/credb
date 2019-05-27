/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"
#include <json/json.h>

namespace credb
{

template <typename T> class PendingSetResponse : public PendingMessage
{
private:
    std::set<T> m_result;

public:
    PendingSetResponse(operation_id_t id, ClientImpl &client) : PendingMessage(id, client) {}

    auto result() -> decltype(m_result) && { return std::move(m_result); }

    void parse(bitstream &msg) override
    {
        uint32_t num_values = 0;
        msg >> num_values;

        for(uint32_t i = 0; i < num_values; ++i)
        {
            T t;
            msg >> t;
            m_result.emplace(std::move(t));
        }
    }
};
} // namespace credb
