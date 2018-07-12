#pragma once

#include <stdint.h>

namespace credb
{

enum class FunctionCallResult : uint8_t
{
    Unknown,
    Success,
    ExecutionLimitReached,
    PolicyRejected,
    ProgramFailure
};

}
