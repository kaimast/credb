#pragma once

#include <functional>
#include "util/hash.h"
#include "credb/event_id.h"

namespace credb
{

struct event_id_hasher
{
    size_t operator()(const credb::event_id_t &eid) const
    {
        return hash_buffer(reinterpret_cast<const uint8_t*>(&eid), sizeof(eid));
    }
};

} // namespace credb
