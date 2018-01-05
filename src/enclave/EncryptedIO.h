#pragma once

#ifdef FAKE_ENCLAVE
#include "credb/ucrypto/ucrypto.h"
#else
#include <sgx_tcrypto.h>
#endif

#include <bitstream.h>
#include <string>

namespace credb
{
namespace trusted
{

class EncryptedIO
{
    sgx_aes_gcm_128bit_key_t m_disk_key;

public:
    virtual ~EncryptedIO() {}
    const sgx_aes_gcm_128bit_key_t &disk_key() const { return m_disk_key; }
    void set_disk_key(const sgx_aes_gcm_128bit_key_t &key);
    [[nodiscard]] virtual bool read_from_disk(const std::string &filename, bitstream &data);
    [[nodiscard]] virtual bool write_to_disk(const std::string &filename, const bitstream &data);
    [[nodiscard]] bool decrypt_disk(uint8_t *buffer, int32_t size, bitstream &bstream);
};

} // namespace trusted
} // namespace credb
