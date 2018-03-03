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

    void set_disk_key(const sgx_aes_gcm_128bit_key_t &key)
    {
        memcpy(&m_disk_key, &key, sizeof(sgx_aes_gcm_128bit_key_t));
    }

    virtual bool is_remote() const = 0;

    /**
     * Get the number of files on disk
     *
     * @note this is an untrusted function
     */
    virtual size_t num_files() = 0;

    /**
     * Get the approximate size of all files on disk
     *
     * @note this is an untrusted function
     */
    virtual size_t total_file_size() = 0;

    /**
     * Read a file from disk and decrypt the content
     */
    [[nodiscard]] virtual bool read_from_disk(const std::string &filename, bitstream &data) = 0;

    /**
     * Encrypt and write a file to disk
     */
    [[nodiscard]] virtual bool write_to_disk(const std::string &filename, const bitstream &data) = 0;

    /**
     * Decrypt a buffer manually
     *
     * This is only needed for downstream mode
     */
    [[nodiscard]] bool decrypt_disk(uint8_t *buffer, int32_t size, bitstream &bstream);

    sgx_aes_gcm_128bit_key_t& disk_key()
    {
        return m_disk_key;
    }
 
private:
    sgx_aes_gcm_128bit_key_t m_disk_key;
};

} // namespace trusted
} // namespace credb
