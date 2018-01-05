#pragma once

#include "PendingMessage.h"
#include "credb/Witness.h"

namespace credb
{

class PendingWitnessResponse : public PendingMessage
{
public:
    PendingWitnessResponse(uint32_t msg_id, ClientImpl &client, Witness &witness)
    : PendingMessage(msg_id, client), m_success(false), m_witness(witness)
    {
    }

    PendingWitnessResponse(const PendingWitnessResponse &other) = delete;


    bool success() const { return m_success; }

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
    bool m_success;
    Witness &m_witness;
};

} // namespace credb
