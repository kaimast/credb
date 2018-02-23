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
public:
    virtual ~EncryptedIO() = default;

    const sgx_aes_gcm_128bit_key_t &disk_key() const { return m_disk_key; }
    void set_disk_key(const sgx_aes_gcm_128bit_key_t &key);

    /**
     * Get the number of files on disk
     *
     * @note this is an untrusted function
     */
    size_t num_files();

    /**
     * Get the approximate size of all files on disk
     *
     * @note this is an untrusted function
     */
    size_t total_file_size();

    /**
     * Read a file from disk and decrypt the content
     */
    [[nodiscard]] virtual bool read_from_disk(const std::string &filename, bitstream &data);

    /**
     * Encrypt and write a file to disk
     */
    [[nodiscard]] virtual bool write_to_disk(const std::string &filename, const bitstream &data);

    /**
     * Decrypt a buffer manually
     *
     * This is only needed for downstream mode
     */
    [[nodiscard]] bool decrypt_disk(uint8_t *buffer, int32_t size, bitstream &bstream);

private:
    sgx_aes_gcm_128bit_key_t m_disk_key;
};

} // namespace trusted
} // namespace credb
