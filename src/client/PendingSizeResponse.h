/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"

namespace credb
{

class PendingSizeResponse : public PendingMessage
{
public:
    PendingSizeResponse(uint32_t msg_id, ClientImpl &client)
    : PendingMessage(msg_id, client), m_size(0)
    {
    }

    uint32_t size() const { return m_size; }

protected:
    void parse(bitstream &msg) override { msg >> m_size; }

private:
    uint32_t m_size;
};

} // namespace credb
