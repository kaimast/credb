/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"

namespace credb
{

class PendingBitstreamResponse : public PendingMessage
{
public:
    PendingBitstreamResponse(uint32_t msg_id, ClientImpl &client, bitstream &bstream)
    : PendingMessage(msg_id, client), m_bstream(bstream)
    {
    }

    PendingBitstreamResponse(const PendingBitstreamResponse &other) = delete;

protected:
    void parse(bitstream &msg) override { msg >> m_bstream; }

private:
    bitstream &m_bstream;
};

} // namespace credb
