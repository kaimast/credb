/** @file */

#pragma once

#include <algorithm>
#include <cstring>
#include <stdint.h>
#include <string>
#include <tuple>


namespace credb
{

/// Unique identifier for a shard
typedef uint16_t shard_id_t;

/// Unique identifier for a block in a shard
typedef uint32_t block_id_t; // FIXME: = page_no_t

/// Indicator of how many changes where made to an object
typedef uint32_t version_number_t;

/// Unique identifier for an event inside a block
typedef uint16_t event_index_t;

/// Constant for a invalid version
constexpr version_number_t INVALID_VERSION_NO = 0;

/// Constant for the initial version
constexpr version_number_t INITIAL_VERSION_NO = 1;

/// Constant for an Invalid block
constexpr block_id_t INVALID_BLOCK = 0;

/// Constant for the initial block in a shard
constexpr block_id_t INITIAL_BLOCK = 1;

/**
 * @brief Specifies the position of an event on a node's ledger
 */
struct event_id_t
{
    event_id_t() = default;

    /**
     * Explicitly construct the event identifier
     */
    constexpr event_id_t(shard_id_t shard_, block_id_t block_, event_index_t index_)
    : shard(shard_), index(index_), block(block_)
    {
    }

    /// Identifier of the shard (horizontal position)
    shard_id_t shard;

    /// Offset inside the block (vertical position)
    event_index_t index;

    /// Block identifier (vertical position)
    block_id_t block;

    /**
     * Test whether or not the identifier is valid
     */
    explicit operator bool() const { return block != INVALID_BLOCK; }

}__attribute__((packed));

/**
 * Specifies an interval on the timeline of events
 */
struct event_range_t
{
    /**
     * Shard identifier (horizontal position). The range can only cover one specific shard.
     */
    shard_id_t shard;

    /// Identifier of the first block on the range
    block_id_t start_block;

    /// Identifier of the last block on the range
    block_id_t end_block;

    /// Offset on the first block
    event_index_t start_index;

    /// Offset on the last block
    event_index_t end_index;
};

/**
 * Constant for an invalid event identifier
 */
constexpr event_id_t INVALID_EVENT = { static_cast<shard_id_t>(INVALID_BLOCK), INVALID_BLOCK,
                                       static_cast<event_index_t>(0) };

/**
 * Constant for an identifier for an uncommitted event 
 *
 * @note this is only used in transactions
 */
constexpr event_id_t UNCOMMITTED_EVENT = { static_cast<shard_id_t>(-2), static_cast<block_id_t>(-2),
                                           static_cast<event_index_t>(-2) };

#ifdef TEST
inline ::std::ostream &operator<<(::std::ostream &os, const event_id_t &eid)
{
    return os << "event_id_t{shard=" << eid.shard << ", block=" << eid.block << ", index=" << eid.index << "}";
}
#endif

/**
 * Compare two event identifier for equality
 */
inline bool operator==(const event_id_t &lhs, const event_id_t &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(event_id_t)) == 0;
}

/**
 * Compare two event identifiers for inequality
 */
inline bool operator!=(const event_id_t &lhs, const event_id_t &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(event_id_t)) != 0;
}

/**
 * @brief Result of an order query
 */
enum class OrderResult
{
    Unknown,
    Equal,
    OlderThan,
    NewerThan,
    Concurrent
};

/**
 * @brief Order two event identifiers with respect to each other
 */
inline OrderResult order(const event_id_t &first, const event_id_t &second)
{
    // FIXME detect whether events are on the same server

    if(first.shard != second.shard)
        return OrderResult::Unknown;

    if(first.block < second.block)
        return OrderResult::OlderThan;
    else if(first.block == second.block && first.index < second.index)
        return OrderResult::OlderThan;
    else if(first == second)
        return OrderResult::Equal;
    else
        return OrderResult::NewerThan;
}

} // namespace credb
