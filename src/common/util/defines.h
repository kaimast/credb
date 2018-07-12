#pragma once

#include <stdint.h>
#include <string>

#include "types.h"

typedef uint32_t page_no_t;
constexpr page_no_t INVALID_PAGE_NO = static_cast<page_no_t>(-1);

typedef uint32_t transaction_id_t;
constexpr transaction_id_t INVALID_TX_ID = 0;

const size_t SAMPLE_SP_IV_SIZE = 12;

enum sp_ra_msg_status_t
{
    SP_OK,
    SP_UNSUPPORTED_EXTENDED_EPID_GROUP,
    SP_INTEGRITY_FAILED,
    SP_QUOTE_VERIFICATION_FAILED,
    SP_IAS_FAILED,
    SP_INTERNAL_ERROR,
    SP_PROTOCOL_ERROR,
    SP_QUOTE_VERSION_ERROR,
};

enum class SetOperation
{
    Union,
    Intersect,
};

/*Key Derivation Function ID : 0x0001  AES-CMAC Entropy Extraction and Key Expansion*/
const uint16_t SAMPLE_AES_CMAC_KDF_ID = 0x0001;

const uint16_t CLIENT_PORT = 5042;
const uint16_t SERVER_PORT = 5043;

const uint32_t MAX_BUF_LEN = 1024;

inline std::string to_string(int i, int min = -1)
{
    char const digit[] = "0123456789";
    std::string out = "";
    int32_t pos = -1;

    if(i < 0)
    {
        out = "-";
        pos += 1;
        i *= -1;
    }

    int shifter = i;

    do
    {
        pos += 1;
        shifter = shifter / 10;
    } while(shifter || pos < min - 1);

    out.resize(pos + 1);

    do
    { // Move back, inserting digits as u go
        out[pos] = digit[i % 10];
        pos -= 1;
        i = i / 10;
    } while(i);

    while(pos >= 0)
    {
        out[pos] = '0';
        pos -= 1;
    }

    return out;
}

template <typename T> inline size_t get_unordered_container_value_byte_size(const T &map)
{
    return map.bucket_count() * sizeof(typename T::value_type);
}
