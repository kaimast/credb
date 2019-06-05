/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include <array>

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
    std::array<uint8_t, SGX_AESGCM_MAC_SIZE> tag;
    assert(static_cast<size_t>(size) >= tag.size());

    memcpy(reinterpret_cast<void*>(tag.data()), reinterpret_cast<void*>(buffer), tag.size());

    uint8_t *data = buffer + tag.size();
    uint32_t real_size = size - tag.size();

    bstream.resize(real_size);

    std::array<uint8_t, SAMPLE_SP_IV_SIZE> aes_gcm_iv = { 0 };

    auto ret = sgx_rijndael128GCM_decrypt(&m_disk_key, data, real_size, bstream.data(), aes_gcm_iv.data(), SAMPLE_SP_IV_SIZE, nullptr, 0, reinterpret_cast<const sgx_aes_gcm_128bit_tag_t *>(tag.data()));

    if(ret != SGX_SUCCESS)
    {
        log_error("failed to sgx_rijndael128GCM_decrypt");
        return false;
    }
#else
    bstream.resize(size);
    memcpy(reinterpret_cast<void*>(bstream.data()),
            reinterpret_cast<void*>(buffer), size);
#endif

    return true;
}

} // namespace credb::trusted
