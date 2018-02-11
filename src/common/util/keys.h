#pragma once

#include <stdint.h>

#include "ecp.h"

#ifdef IS_ENCLAVE
#include <sgx_tcrypto.h>
#else
#include "credb/ucrypto/ucrypto.h"
#endif

#ifndef SAMPLE_FEBITSIZE
#define SAMPLE_FEBITSIZE 256
#endif

#define SAMPLE_ECP_KEY_SIZE (SAMPLE_FEBITSIZE / 8)

constexpr size_t EC_MAC_SIZE = 16;
constexpr size_t MAC_KEY_SIZE = 16;

typedef uint8_t ec_key_128bit_t[EC_MAC_SIZE];

const char str_SMK[] = "SMK";
const char str_SK[] = "SK";
const char str_MK[] = "MK";
const char str_VK[] = "VK";

inline size_t EC_DERIVATION_BUFFER_SIZE(size_t label_length) { return label_length + 4; }

/**
 * @brief Parses the full path of an object
 *
 * A path might either be just a key
 * or a key + a field identifier.
 */
inline std::pair<std::string, std::string> parse_path(const std::string &full_path)
{
    std::string key, path;
    size_t ppos;

    if((ppos = full_path.find(".")) != std::string::npos)
    {
        key = full_path.substr(0, ppos);
        path = full_path.substr(ppos + 1, std::string::npos);
    }
    else
    {
        key = full_path;
    }

    return {key, path};
}

 
