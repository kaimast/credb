#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "PendingMessage.h"
#include "credb/defines.h"
#include <json/json.h>

namespace credb
{

class PendingInternalFindResponse : public PendingMessage
{
private:
    std::vector<std::tuple<std::string, event_id_t, json::Document>> m_result;

public:
    PendingInternalFindResponse(operation_id_t id, ClientImpl &client) : PendingMessage(id, client)
    {
    }

    auto result() -> decltype(m_result) && { return std::move(m_result); }

    void parse(bitstream &msg) override
    {
        uint32_t num_values = 0;
        msg >> num_values;

        for(uint32_t i = 0; i < num_values; ++i)
        {
            std::string key;
            event_id_t eid;
            msg >> key >> eid;
            m_result.emplace_back(key, eid, json::Document(msg));
        }
    }
};

class PendingFindResponse : public PendingMessage
{
private:
    std::vector<std::tuple<std::string, json::Document>> m_result;

public:
    PendingFindResponse(operation_id_t id, ClientImpl &client) : PendingMessage(id, client) {}

    auto result() -> decltype(m_result) && { return std::move(m_result); }

    void parse(bitstream &msg) override
    {
        uint32_t num_values = 0;
        msg >> num_values;

        for(uint32_t i = 0; i < num_values; ++i)
        {
            std::string key;
            event_id_t eid;
            msg >> key >> eid;
            (void)eid; // not used
            m_result.emplace_back(key, json::Document(msg));
        }
    }
};
} // namespace credb
