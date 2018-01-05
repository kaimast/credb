#pragma once

#include "PendingMessage.h"
#include "credb/defines.h"

namespace credb
{

class PendingBooleanResponse : public PendingMessage
{
public:
    PendingBooleanResponse(uint32_t msg_id, ClientImpl &client)
    : PendingMessage(msg_id, client), m_success(false)
    {
    }

    bool success() const { return m_success; }

protected:
    void parse(bitstream &msg) override { msg >> m_success; }

private:
    bool m_success;
};

} // namespace credb
