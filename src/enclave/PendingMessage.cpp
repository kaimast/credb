/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "PendingMessage.h"
#include "Peer.h"

namespace credb::trusted
{

PendingMessage::PendingMessage(operation_id_t op_id, Peer &peer)
: m_has_message(false), m_operation_id(op_id), m_peer(peer)
{
    peer.increment_pending_count();
}

PendingMessage::PendingMessage(PendingMessage &&other) noexcept
: m_has_message(other.m_has_message), m_operation_id(other.m_operation_id), m_peer(other.m_peer)
{
}

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

        m_peer.decrement_pending_count();

        return true;
    }
}

} // namespace credb::trusted
