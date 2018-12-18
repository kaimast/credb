/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#include "PendingMessage.h"
#include "ClientImpl.h"

namespace credb
{

void PendingMessage::wait()
{
    if(!m_has_message)
    {
        auto msg = m_client.receive_response(m_operation_id);
        this->parse(msg);
        m_has_message = true;
    }
}

} // namespace credb
