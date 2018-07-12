#pragma once
#include <string>

namespace credb
{

template <typename T> inline size_t get_heap_size_of(const T &t)
{
    // Sizeof only works correctly with POD types
    static_assert(std::is_pod<T>());
    return sizeof(t);
}

template <> inline size_t get_heap_size_of<std::string>(const std::string &str)
{
    return str.size();
}

} // namespace credb
