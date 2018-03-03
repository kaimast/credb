#include "LocalEncryptedIO.h"
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

bool LocalEncryptedIO::read_from_disk(const std::string &filename, bitstream &data)
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

bool LocalEncryptedIO::write_to_disk(const std::string &filename, const bitstream &data)
{
    bool result = false;

#if defined(ENCRYPT_FILES) && !defined(FAKE_ENCLAVE)
    size_t buffer_size = data.size() + sizeof(sgx_aes_gcm_128bit_tag_t);

    auto buffer = new uint8_t[buffer_size];
    auto tag = reinterpret_cast<sgx_aes_gcm_128bit_tag_t *>(buffer);
    auto cdata = buffer + sizeof(*tag);

    uint8_t aes_gcm_iv[SAMPLE_SP_IV_SIZE] = { 0 };

    auto ret = sgx_rijndael128GCM_encrypt(&disk_key(), data.data(), data.size(), cdata,
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

size_t LocalEncryptedIO::num_files()
{
    size_t result = 0;

    ::get_num_files(&result);

    return result;
}

size_t LocalEncryptedIO::total_file_size()
{
    size_t result = 0;

    ::get_total_file_size(&result);

    return result;
}

} // namespace trusted
} // namespace credb
