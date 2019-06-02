/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "EncryptedIO.h"
#include "logging.h"
#include "util/defines.h"
#include "util/error.h"

#ifdef FAKE_ENCLAVE
#include "../src/server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#endif

namespace credb::trusted
{

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

} // namespace credb::trusted
