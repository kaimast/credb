#pragma once

#include <stdint.h>

enum class MessageType : uint8_t
{
    TellGroupId = 0,
    GroupIdResponse = 1,
    AttestationMessage1 = 2,
    AttestationMessage2 = 3,
    AttestationMessage3 = 4,
    AttestationResult = 5,
    OperationRequest = 6,
    OperationResponse = 7,
    ForwardedOperationRequest = 8,
    PushIndexUpdate = 9,
    NotifyTrigger = 10
};

typedef uint8_t mtype_data_t;
