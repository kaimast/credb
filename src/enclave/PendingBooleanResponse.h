#pragma once

#include "PendingMessage.h"

namespace credb
{
namespace trusted
{

class PendingBooleanResponse : public PendingMessage
{
public:
    PendingBooleanResponse(operation_id_t msg_id, Peer &peer)
    : PendingMessage(msg_id, peer), m_success(false)
    {
    }

    PendingBooleanResponse(PendingBooleanResponse &&other)
    : PendingMessage(std::move(other)), m_success(other.m_success)
    {
    }

    bool success() const { return m_success; }

protected:
    void parse(bitstream &msg) override { msg >> m_success; }

private:
    bool m_success;
};

class PendingEventIdResponse : public PendingMessage
{
public:
    PendingEventIdResponse(uint32_t msg_id, Peer &peer)
    : PendingMessage(msg_id, peer), m_event_id(INVALID_EVENT)
    {
    }

    bool success() const { return static_cast<bool>(m_event_id); }

    const event_id_t &event_id() const { return m_event_id; }

protected:
    void parse(bitstream &msg) override { msg >> m_event_id; }

private:
    event_id_t m_event_id;
};

} // namespace trusted
} // namespace credb
