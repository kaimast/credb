#pragma once

#include "PendingMessage.h"
#include "credb/Witness.h"

namespace credb
{
namespace trusted
{

class PendingWitnessResponse : public PendingMessage
{
public:
    PendingWitnessResponse(operation_id_t msg_id, Peer &peer)
    : PendingMessage(msg_id, peer)
    {
    }

    bool success() const
    {
        return m_success;
    }

    Witness get_witness()
    {
       return std::move(m_witness);
    }

protected:
    void parse(bitstream &msg) override
    {
        msg >> m_success;

        if(m_success)
        {
            msg >> m_witness;
        }
    }

private:
    bool m_success = false;
    Witness m_witness;
};

}
} // namespace credb
