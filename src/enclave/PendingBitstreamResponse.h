#pragma once

#include <bitstream.h>
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

    /**
     * Once we waited successfully for a message
     * this will return the result bitstream
     */
    bitstream get_result()
    {
        return std::move(m_bitstream);
    }

protected:
    void parse(bitstream &msg) override { msg >> m_bitstream; }

private:
    bitstream m_bitstream;
};

} // namespace trusted
} // namespace credb
