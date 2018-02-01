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
    /**
     * Represents a future message to be received from a peer
     * 
     * @note only call this while holding the lock on peer
     */
    PendingMessage(operation_id_t op_id, Peer &peer);

    /**
     * Move constructor
     */
    PendingMessage(PendingMessage &&other) noexcept;

    virtual ~PendingMessage() {}

    bool has_message() const { return m_has_message; }

    Peer &peer() { return m_peer; }

    /**
     * @brief Waits until the message has been received
     * @note only call this while holding the lock on peer
     */
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
