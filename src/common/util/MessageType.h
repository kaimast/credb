#pragma once

#include <stdint.h>

enum class MessageType : uint8_t
{
    TellGroupId,
    GroupIdResponse,
    AttestationMessage1,
    AttestationMessage2,
    AttestationMessage3,
    AttestationResult,
    OperationRequest,
    OperationResponse,
    ForwardedOperationRequest,
    PushIndexUpdate,
    NotifyTrigger
};

typedef uint8_t mtype_data_t;
