#pragma once

#include "util/OperationType.h"
#include <bitstream.h>
#include <stdint.h>

namespace credb
{
namespace trusted
{

class Peer;

class PendingMessage
{
public:
    PendingMessage(operation_id_t op_id, Peer &peer)
    : m_has_message(false), m_operation_id(op_id), m_peer(peer)
    {
    }

    PendingMessage(PendingMessage &&other)
    : m_has_message(other.m_has_message), m_operation_id(other.m_operation_id), m_peer(other.m_peer)
    {
    }

    virtual ~PendingMessage() {}

    bool has_message() const { return m_has_message; }

    Peer &peer() { return m_peer; }

    /// Waits until the message has been received
    bool wait(bool block = true);

protected:
    virtual void parse(bitstream &msg) = 0;

private:
    bool m_has_message;

    uint32_t m_operation_id;
    Peer &m_peer;
};

} // namespace trusted
} // namespace credb
