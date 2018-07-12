#pragma once

#include "MurmurHash2.h"
#include <cstdint>
#include <string>
#include <utility>

// Using MurmurHash64A as current hash function.
// Not using MurmurHash3_x64_128 because 128 bit result is a little bit annoying,
// although it's faster than 64bit MurmurHash2.
// Also notice that there are other versions of MurmurHash optimized for different
// platforms (x86/x64, big-endian/little-endian).
// Currently, using a fixed seed generated from random.org.
// MurmurHash is a widely hash function series. GCC also use it.
// see: https://github.com/gcc-mirror/gcc/blob/master/libstdc++-v3/libsupc++/hash_bytes.cc
// If C++17 suuport comes to SGX, maybe consider using std::hash on std::string_view.

typedef uint64_t hashval_t;

inline hashval_t hash_buffer(const uint8_t *array, uint32_t size)
{
    constexpr uint64_t seed = 0x4126f4492047c28full;
    return MurmurHash64A(array, static_cast<int>(size), seed);
}

template <typename T> inline hashval_t hash(const T &t)
{
    return hash_buffer(reinterpret_cast<const uint8_t *>(&t), sizeof(T));
}

template <> inline hashval_t hash<std::string>(const std::string &str)
{
    return hash_buffer(reinterpret_cast<const uint8_t *>(str.c_str()), str.size());
}
