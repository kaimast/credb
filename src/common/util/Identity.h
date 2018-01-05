#pragma once

#include <stdint.h>
#include <string>
#include <stdexcept>

namespace credb
{

enum class IdentityType : uint8_t
{
    Server,
    Client
};

inline constexpr bool runs_in_tee(IdentityType type) { return type == IdentityType::Server; }

struct Identity
{
public:
    Identity() = default;

    Identity(IdentityType type_, const std::string &name_) : type(type_), name(name_) {}

    Identity(Identity &&other) : type(other.type), name(std::move(other.name)) {}

    std::string to_string() const
    {
        if(type == IdentityType::Server)
            return "server://" + name;
        else if(type == IdentityType::Client)
            return "client://" + name;
        else
            throw std::runtime_error("Unknown identity type");
    }

    const IdentityType type;
    const std::string name;
    // TODO signature
};

inline bool operator==(const Identity &first, const Identity &second)
{
    return first.name == second.name;
}

inline bool operator!=(const Identity &first, const Identity &second)
{
    return first.name != second.name;
}

static const Identity INVALID_IDENTITY = { IdentityType::Server, "" };

} // namespace credb
