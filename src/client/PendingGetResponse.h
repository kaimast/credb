/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"
#include "credb/defines.h"
#include <json/json.h>

namespace credb
{

class PendingGetResponse : public PendingMessage
{
public:
    PendingGetResponse(uint32_t msg_id, ClientImpl &client)
    : PendingMessage(msg_id, client), m_event_id(INVALID_EVENT), m_document()
    {
    }

    ~PendingGetResponse() {}

    bool success() const { return static_cast<bool>(m_event_id); }

    const event_id_t &event_id() const { return m_event_id; }

    json::Document document() { return std::move(m_document); }

protected:
    void parse(bitstream &msg) override
    {
        msg >> m_event_id;

        if(success())
        {
            msg >> m_document;
        }
    }

private:
    event_id_t m_event_id;
    json::Document m_document;
};

} // namespace credb
