/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include "PendingMessage.h"
#include "util/FunctionCallResult.h"
#include <cowlang/Value.h>

namespace credb
{

class PendingCallResponse : public PendingMessage
{
public:
    PendingCallResponse(uint32_t msg_id, ClientImpl &client, cow::MemoryManager &mem)
        : PendingMessage(msg_id, client), m_mem(mem)
    {
    }

    bool success() const
    {
        return m_result == FunctionCallResult::Success;
    }

    FunctionCallResult result() const
    {
        return m_result;
    }

    cow::ValuePtr return_value()
    {
        return m_return_value;
    }

    const std::string &error() const
    {
        return m_error;
    }

protected:
    void parse(bitstream &msg) override
    {
        msg >> m_result;

        switch(m_result)
        {
        case FunctionCallResult::Success:
            m_return_value = cow::read_value(msg, m_mem);
            break;
        case FunctionCallResult::ExecutionLimitReached:
            break;
        case FunctionCallResult::ProgramFailure:
            msg >> m_error;
            break;
        default:
            m_result = FunctionCallResult::Unknown;
            m_error = "Unknown function call result";
            break;
        }
    }

private:
    cow::MemoryManager &m_mem;

    FunctionCallResult m_result = FunctionCallResult::Unknown;
    cow::ValuePtr m_return_value = nullptr;
    std::string m_error = "";
};

} // namespace credb
