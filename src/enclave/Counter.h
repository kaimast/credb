#pragma once

#include <atomic>

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
        return m_next++;
    }

private:
    std::atomic<T> m_next = 1;
};

} // namespace trusted
} // namespace credb
