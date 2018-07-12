/** @file */

#pragma once

#include <algorithm>
#include <cstring>
#include <stdint.h>
#include <string>
#include <tuple>
#include <unordered_map>

#ifdef TEST
#include <fstream>
#endif


namespace credb
{

/// Unique identifier for a shard
using shard_id_t = uint16_t;

/// Unique identifier for a block in a shard
using block_id_t = uint32_t; //FIXME page_no_t;

/// Indicator of how many changes where made to an object
using version_number_t = uint32_t;

/// Unique identifier for an event inside a block
using block_index_t = uint16_t;

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
    constexpr event_id_t(shard_id_t shard_, block_id_t block_, block_index_t index_)
    : shard(shard_), index(index_), block(block_)
    {
    }

    /// Identifier of the shard (horizontal position)
    shard_id_t shard;

    /// Offset inside the block (vertical position)
    block_index_t index;

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
    /// Identifier of the first block on the range
    block_id_t start_block;

    /// Identifier of the last block on the range
    block_id_t end_block;

    /// Offset on the first block
    block_index_t start_index;

    /// Offset on the last block
    block_index_t end_index;
};

/**
 * Defines the extent of a transaction across the ledger
 */
using transaction_bounds_t = std::unordered_map<shard_id_t, event_range_t>;

/**
 * Constant for an invalid event identifier
 */
constexpr event_id_t INVALID_EVENT = { static_cast<shard_id_t>(INVALID_BLOCK), INVALID_BLOCK,
                                       static_cast<block_index_t>(0) };

/**
 * Constant for an identifier for an uncommitted event 
 *
 * @note this is only used in transactions
 */
constexpr event_id_t UNCOMMITTED_EVENT = { static_cast<shard_id_t>(-2), static_cast<block_id_t>(-2),
                                           static_cast<block_index_t>(-2) };

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
    if(first.shard != second.shard)
    {
        return OrderResult::Unknown;
    }

    if(first.block < second.block)
    {
        return OrderResult::OlderThan;
    }
    else if(first.block == second.block && first.index < second.index)
    {
        return OrderResult::OlderThan;
    }
    else if(first == second)
    {
        return OrderResult::Equal;
    }
    else
    {
        return OrderResult::NewerThan;
    }
}

/**
 * Is an event id older than another?
 */
inline bool operator<(const event_id_t &first, const event_id_t &second)
{
    return order(first, second) == OrderResult::OlderThan;
}

/**
 * Is an event strictly before a specific range? 
 */
inline bool operator<(const event_id_t &e, const event_range_t &range)
{
    if(e.block < range.start_block)
    {
        return true;
    }
    else if(e.block == range.start_block)
    {
        return e.index < range.start_index;
    }
    else
    {
        return false;
    }
}

/**
 * @brief is an event strictly after an event range? 
 */
inline bool operator>(const event_id_t &e, const event_range_t &range)
{
    if(e.block > range.end_block)
    {
        return true;
    }
    else if(e.block == range.end_block)
    {
        return e.index > range.end_index;
    }
    else
    {
        return false;
    }
}

/**
 * @brief Order two event ranges with respect to each other
 */
inline OrderResult order(const event_range_t &first, const event_range_t &second)
{
    if((first.end_block < second.start_block) ||
       (first.end_block == second.start_block && first.end_index < second.start_index))
    {
        return OrderResult::OlderThan;
    }
    else if((first.start_block > second.end_block) ||
            (first.end_block == second.start_block && first.start_index > second.end_index))
    {
        return OrderResult::NewerThan;
    }
    else
    {
        return OrderResult::Concurrent;
    }
}

/**
 * Order two transaction bounds with respect to each other
 *
 * @note this assumes that the transaction bounds are derived from valid, committed transactions
 */
inline OrderResult order(const transaction_bounds_t &first, const transaction_bounds_t &second)
{
    for(auto &[shard, range1] : first)
    {
        auto it = second.find(shard);

        if(it == second.end())
        {
            continue;
        }

        auto range2 = it->second;
        auto res = order(range1, range2);

        if(res == OrderResult::Concurrent)
        {
            // Repeatable reads?
            // throw exception here?
        }

        return res;
    }

    return OrderResult::Unknown;
}

} // namespace credb
