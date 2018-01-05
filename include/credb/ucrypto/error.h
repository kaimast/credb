#pragma once

#include <cstdint>
#include <string>

enum class Status : uint8_t
{
    SUCCESS = 0,
    ERROR_INVALID_PARAMETER,
    ERROR_UNEXPECTED,
    ERROR_OUT_OF_MEMORY,
    ERROR_MAC_MISMATCH,
};

typedef Status status_t;

inline std::string to_string(const Status &status)
{
    std::string msg = "UNKNOWN";

    switch(status)
    {
    case Status::SUCCESS:
        msg = "Success";
        break;
    case Status::ERROR_INVALID_PARAMETER:
        msg = "Invalid parameter";
        break;
    case Status::ERROR_UNEXPECTED:
        msg = "Unexpected error";
        break;
    case Status::ERROR_OUT_OF_MEMORY:
        msg = "Out of memory";
        break;
    case Status::ERROR_MAC_MISMATCH:
        msg = "MACs do not match!";
        break;
    default:
        break;
    }

    return msg + "(" + std::to_string(static_cast<uint8_t>(status)) + ")";
}
