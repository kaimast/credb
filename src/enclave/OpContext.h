/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "util/Identity.h"

namespace credb::trusted
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

    OpContext(OpContext &&other)
        : m_identity(other.m_identity), m_program(other.m_program)
    {
    }

    OpContext duplicate() const
    {
        return OpContext(m_identity, m_program);
    }

    const Identity &identity() const { return m_identity; }

    bool valid() const { return m_identity != INVALID_IDENTITY; }

    bool called_by_program() const { return !m_program.empty(); }

    const std::string &program_name() const { return m_program; }

    std::string to_string() const
    {
        if(called_by_program())
        {
            return identity().to_string() + ":" + program_name();
        }
        else
        {
            return identity().to_string();
        }
    }

private:
    const Identity &m_identity;
    const std::string m_program;
};

} // namespace credb::trusted
