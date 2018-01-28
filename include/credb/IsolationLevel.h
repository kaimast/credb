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

}
