#include "PendingMessage.h"
#include "Peer.h"

namespace credb
{
namespace trusted
{

bool PendingMessage::wait(bool block)
{
    if(m_has_message)
    {
        return true;
    }

    auto msg = m_peer.receive_response(m_operation_id, block);

    if(!msg)
    {
        return false;
    }
    else
    {
        this->parse(*msg);
        m_has_message = true;

        delete msg;
        return true;
    }
}

} // namespace trusted
} // namespace credb
