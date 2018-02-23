#include "EncryptedIO.h"
#include "logging.h"
#include <cstring>

#ifdef FAKE_ENCLAVE
#include "../src/server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#endif

#define ENCRYPT_FILES

namespace credb
{
namespace trusted
{

void EncryptedIO::set_disk_key(const sgx_aes_gcm_128bit_key_t &key)
{
    memcpy(&m_disk_key, &key, sizeof(sgx_aes_gcm_128bit_key_t));
}

bool EncryptedIO::read_from_disk(const std::string &filename, bitstream &data)
{
    int32_t size = 0;

    ::get_file_size(&size, filename.c_str());

    if(size < 0)
    {
        // log_error("No such file: " + ffilename);
        return false;
    }

    bool retval = false;

#if defined(ENCRYPT_FILES) && !defined(FAKE_ENCLAVE)
    auto buffer = new uint8_t[size];
    ::read_from_disk(&retval, filename.c_str(), buffer, size);
    if(!retval)
    {
        log_error("Failed to read file: " + filename);
        return false;
    }

    retval = decrypt_disk(buffer, size, data);
    delete[] buffer;
#else
    data.resize(size);

    ::read_from_disk(&retval, filename.c_str(), data.data(), data.size());

    if(!retval)
    {
        log_error("Failed to read file: " + filename);
    }
#endif

    return retval;
}

bool EncryptedIO::write_to_disk(const std::string &filename, const bitstream &data)
{
    bool result = false;

#if defined(ENCRYPT_FILES) && !defined(FAKE_ENCLAVE)
    size_t buffer_size = data.size() + sizeof(sgx_aes_gcm_128bit_tag_t);

    auto buffer = new uint8_t[buffer_size];
    auto tag = reinterpret_cast<sgx_aes_gcm_128bit_tag_t *>(buffer);
    auto cdata = buffer + sizeof(*tag);

    uint8_t aes_gcm_iv[SAMPLE_SP_IV_SIZE] = { 0 };

    auto ret = sgx_rijndael128GCM_encrypt(&m_disk_key, data.data(), data.size(), cdata,
                                          &aes_gcm_iv[0], SAMPLE_SP_IV_SIZE, nullptr, 0, tag);

    if(ret != SGX_SUCCESS)
    {
        delete[] buffer;
        log_error("failed to sgx_rijndael128GCM_encrypt");
        return false;
    }

    ::write_to_disk(&result, filename.c_str(), buffer, buffer_size);

    delete[] buffer;

#else
    ::write_to_disk(&result, filename.c_str(), data.data(), data.size());
#endif

    return result;
}

size_t EncryptedIO::num_files()
{
    size_t result = 0;

    ::get_num_files(&result);

    return result;
}

size_t EncryptedIO::total_file_size()
{
    size_t result = 0;

    ::get_total_file_size(&result);

    return result;
}

bool EncryptedIO::decrypt_disk(uint8_t *buffer, int32_t size, bitstream &bstream)
{
#if defined(ENCRYPT_FILES) && !defined(FAKE_ENCLAVE)
    uint8_t tag[SGX_AESGCM_MAC_SIZE];
    assert(static_cast<size_t>(size) >= sizeof(tag));
    memcpy(tag, buffer, sizeof(tag));

    uint8_t *data = buffer + sizeof(tag);
    uint32_t real_size = size - sizeof(tag);

    bstream.resize(real_size);
    uint8_t aes_gcm_iv[SAMPLE_SP_IV_SIZE] = { 0 };

    auto ret = sgx_rijndael128GCM_decrypt(&m_disk_key, data, real_size, bstream.data(),
                                          &aes_gcm_iv[0], SAMPLE_SP_IV_SIZE, nullptr, 0,
                                          reinterpret_cast<const sgx_aes_gcm_128bit_tag_t *>(tag));

    if(ret != SGX_SUCCESS)
    {
        log_error("failed to sgx_rijndael128GCM_decrypt");
        return false;
    }
#else
    bstream.resize(size);
    memcpy(bstream.data(), buffer, size);
#endif

    return true;
}


} // namespace trusted
} // namespace credb
