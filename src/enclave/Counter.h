#pragma once

#include "util/Mutex.h"

namespace credb
{
namespace trusted
{

/// A simple thread-safe counter
template <typename T> class Counter
{
public:
    T next()
    {
        std::lock_guard<credb::Mutex> lock(m_mutex);
        
        auto res = m_next;
        m_next += 1;
        return res;
    }

private:
    T m_next = 1;
    credb::Mutex m_mutex;
};

} // namespace trusted
} // namespace credb
