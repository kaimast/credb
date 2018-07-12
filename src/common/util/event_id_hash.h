#pragma once

#include <functional>
#include "util/hash.h"
#include "credb/event_id.h"

namespace std
{

template <>
struct hash<credb::event_id_t>
{
    size_t operator()(const credb::event_id_t &eid) const
    {
        return hash_buffer(reinterpret_cast<const uint8_t*>(&eid), sizeof(eid));
    }
};

} // namespace std
