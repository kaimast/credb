#pragma once
#include <cstdint>

typedef uint8_t etype_data_t;

enum class EncryptionType : etype_data_t
{
    PlainText,
    Encrypted,
    Attestation,
};
