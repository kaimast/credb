/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "util/OperationType.h"
#include <bitstream.h>
#include <stdint.h>

namespace credb
{
class ClientImpl;

class PendingMessage
{
public:
    PendingMessage(operation_id_t op_id, ClientImpl &client)
    : m_has_message(false), m_operation_id(op_id), m_client(client)
    {
    }

    PendingMessage(const PendingMessage &other) = delete;

    virtual ~PendingMessage() {}

    bool has_message() const { return m_has_message; }

    void wait();

protected:
    virtual void parse(bitstream &msg) = 0;

private:
    bool m_has_message;

    uint32_t m_operation_id;
    ClientImpl &m_client;
};

} // namespace credb
