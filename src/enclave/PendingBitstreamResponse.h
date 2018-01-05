#pragma once

#include "PendingMessage.h"

namespace credb
{
namespace trusted
{

class PendingBitstreamResponse : public PendingMessage
{
public:
    PendingBitstreamResponse(uint32_t msg_id, Peer &peer)
    : PendingMessage(msg_id, peer), m_bitstream()
    {
    }

    void move(bitstream &bstream) { bstream = std::move(m_bitstream); }

protected:
    void parse(bitstream &msg) override { msg >> m_bitstream; }

private:
    bitstream m_bitstream;
};

} // namespace trusted
} // namespace credb
