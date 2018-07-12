#pragma once

#include "PendingMessage.h"
#include "credb/defines.h"

namespace credb
{

class PendingEventIdResponse : public PendingMessage
{
public:
    PendingEventIdResponse(uint32_t msg_id, ClientImpl &client)
    : PendingMessage(msg_id, client), m_id(INVALID_EVENT)
    {
    }

    bool success() const { return static_cast<bool>(m_id); }

    const event_id_t &event_id() const { return m_id; }

protected:
    void parse(bitstream &msg) override { msg >> m_id; }

private:
    event_id_t m_id;
};

} // namespace credb
