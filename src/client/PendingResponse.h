#pragma once 

#include "PendingMessage.h"
#include "credb/defines.h"

namespace credb
{

template<typename T>
class PendingResponse : public PendingMessage
{
public:
    PendingResponse(uint32_t msg_id, ClientImpl &client)
        : PendingMessage(msg_id, client)
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
using PendingOrderResponse = PendingResponse<OrderResult>;

} // namespace credb
