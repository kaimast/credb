/** @file */

#pragma once

namespace credb
{

/**
 * @label{IsolationLevel}
 * @brief Enumeration used to specify which Isolation Level a Transaction should use
 */
enum class IsolationLevel : uint8_t
{
    ReadCommitted,
    RepeatableRead,
    Serializable,
};

#ifndef IS_ENCLAVE
/**
 * @brief Convert isolation level into a human-readable string
 */
inline std::ostream& operator<<(std::ostream &stream, const IsolationLevel &level)
{
    switch(level)
    {
    case IsolationLevel::ReadCommitted:
        stream << "ReadCommitted";
        break;
    case IsolationLevel::RepeatableRead:
        stream << "RepeatableRead";
        break;
    case IsolationLevel::Serializable:
        stream << "Serializable";
        break;
    default:
        throw std::runtime_error("Invalid isolation level");
    }

    return stream;
}
#endif

}
