#pragma once

#include "credb/event_id.h"

namespace credb
{
namespace trusted
{

struct ledger_pos_t
{
    block_id_t block;
    block_index_t index;
};

inline bool operator==(const ledger_pos_t &first, const ledger_pos_t &second)
{
    return first.block == second.block && first.index == second.index;
}

inline bool operator!=(const ledger_pos_t &first, const ledger_pos_t &second)
{
    return !(first == second);
}

inline OrderResult order(const ledger_pos_t &first, const ledger_pos_t &second)
{
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


}
}
