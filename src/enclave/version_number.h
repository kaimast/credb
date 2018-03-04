#pragma once

#include <stdint.h>
#include <string>

namespace credb
{
namespace trusted
{

/**
 * Version number type that can safely wrap around
 */
class version_number
{
public:
    static constexpr uint16_t MAX_VALUE   = UINT_LEAST16_MAX;
    static constexpr uint16_t WINDOW_SIZE = MAX_VALUE / 2;

    version_number(uint16_t val)
        : m_value(val)
    {
    }

    version_number() = default;

    uint16_t operator() () const
    {
        return m_value;
    }
    
    void increment()
    {
        m_value = (m_value + 1) % MAX_VALUE;
    }

private:
    uint16_t m_value;
};

inline bool operator==(const version_number &first, const version_number &second)
{
    return first() == second();
}

inline bool operator!=(const version_number &first, const version_number &second)
{
    return !(first == second);
}

inline bool operator<(const version_number &first, const version_number &second)
{
    if(first == second)
    {
        return false;
    }
    else if(first() < second())
    {
        auto diff = second() - first();
        return diff < version_number::WINDOW_SIZE;
    }
    else
    {
        auto diff = first() - second();
        return diff > version_number::WINDOW_SIZE;
    }
}

}
}

namespace std
{

inline std::string to_string(const credb::trusted::version_number &no)
{
    return to_string(no());
}

}


