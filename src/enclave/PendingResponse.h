/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once 

#include "PendingMessage.h"

namespace credb
{
namespace trusted
{

template<typename T>
class PendingResponse : public PendingMessage
{
public:
    PendingResponse(uint32_t msg_id, Peer &peer)
    : PendingMessage(msg_id, peer)
    {
    }

    T result() const
    {
        return m_result;
    }

protected:
    void parse(bitstream &msg) override { msg >> m_result; }

private:
    T m_result;
};

using PendingBooleanResponse = PendingResponse<bool>;

}
} // namespace credb
