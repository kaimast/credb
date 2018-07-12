#pragma once

#include <cstring>
#include <stdexcept>
#include <stdlib.h>
#include <string>

#ifdef TEST
#include <iostream>
#endif

#ifdef IS_ENCLAVE
#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#endif
#endif

#include <random>

#include "event_id.h"

/**
 * Convert a value hexadecimal number back to decimal numbers
 *
 * For example, to_hex('B') = 11
 */
inline uint8_t from_hex(char c)
{
    switch(c)
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'A':
        return 10;
    case 'B':
        return 11;
    case 'C':
        return 12;
    case 'D':
        return 13;
    case 'E':
        return 14;
    case 'F':
        return 15;
    default:
        throw std::runtime_error("Not a hexidecimal number");
    }
}

/**
 * Convert a value (in range [0;15]) to hexadecimal.
 *
 * For example, to_hex(10) = 'A'
 */
static inline char to_hex(uint8_t val)
{
    if(val >= 16)
    {
        throw std::runtime_error("Cannot convert value >=16 to hexadecimal");
    }

    switch(val)
    {
    case 0:
        return '0';
    case 1:
        return '1';
    case 2:
        return '2';
    case 3:
        return '3';
    case 4:
        return '4';
    case 5:
        return '5';
    case 6:
        return '6';
    case 7:
        return '7';
    case 8:
        return '8';
    case 9:
        return '9';
    case 10:
        return 'A';
    case 11:
        return 'B';
    case 12:
        return 'C';
    case 13:
        return 'D';
    case 14:
        return 'E';
    case 15:
        return 'F';
    default:
        return 'x';
    }
}

namespace credb
{

enum class ObjectEventType
{
    NewVersion,
    Read,
    Deletion
};


//TODO this shouldn't be part of the public interface
template <typename T> inline T rand()
{
    T randval;

#ifdef IS_ENCLAVE
    sgx_read_rand(reinterpret_cast<unsigned char *>(&randval), sizeof(randval));
#else
    randval = ::rand();
#endif

    return randval;
}

inline std::string random_object_key(size_t length)
{
    if(length <= 0)
        throw std::runtime_error("Key length has to be > 0");

    auto randchar = []() -> char {
        const char charset[] = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[rand<char>() % max_index];
    };

    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

/**
 * @brief This exception will be thrown in the case of stale data being detected on the server side
 */
class StalenessDetectedException : public std::exception
{
    std::string m_what;

public:
    /// Constructor
    StalenessDetectedException(const std::string &what) : m_what(what) {}

    /// Get the exception description
    virtual const char *what() const throw() { return m_what.c_str(); }
};

} // namespace credb
