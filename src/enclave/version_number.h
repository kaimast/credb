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
    using int_type = uint32_t;

    static constexpr int_type MAX_VALUE   = UINT_LEAST32_MAX;
    static constexpr int_type WINDOW_SIZE = MAX_VALUE / 2;

    version_number(int_type val)
        : m_value(val)
    {
    }

    version_number() = default;

    int_type operator() () const
    {
        return m_value;
    }
    
    void increment()
    {
        m_value = (m_value + 1) % MAX_VALUE;
    }

private:
    int_type m_value;
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


