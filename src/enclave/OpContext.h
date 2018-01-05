#pragma once

#include "util/Identity.h"

namespace credb
{
namespace trusted
{

/**
 * @brief Holds information about who/what is invoking the operation
 */
class OpContext
{
public:
    explicit OpContext(const Identity &identity, const std::string &program = "")
    : m_identity(identity), m_program(program)
    {
    }

    const Identity &identity() const { return m_identity; }

    bool valid() const { return m_identity != INVALID_IDENTITY; }

    bool called_by_program() const { return m_program.size() > 0; }

    const std::string &program_name() const { return m_program; }

    std::string to_string() const
    {
        if(called_by_program())
            return identity().to_string() + ":" + program_name();
        else
            return identity().to_string();
    }

private:
    const Identity &m_identity;
    const std::string m_program;
};

} // namespace trusted
} // namespace credb
