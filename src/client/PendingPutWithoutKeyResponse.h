#pragma once

#include "PendingMessage.h"
#include "credb/defines.h"

namespace credb
{

class PendingPutWithoutKeyResponse : public PendingMessage
{
public:
    PendingPutWithoutKeyResponse(uint32_t msg_id, ClientImpl &client)
    : PendingMessage(msg_id, client), m_id(INVALID_EVENT)
    {
    }

    bool success() const { return static_cast<bool>(m_id); }

    const std::string key() const { return m_key; }

    const event_id_t &event_id() const { return m_id; }

protected:
    void parse(bitstream &msg) override { msg >> m_id >> m_key; }

private:
    event_id_t m_id;
    std::string m_key;
};

} // namespace credb
