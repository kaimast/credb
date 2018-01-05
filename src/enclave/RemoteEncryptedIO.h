#pragma once
#include "Enclave.h"
#include "EncryptedIO.h"

namespace credb
{
namespace trusted
{

class RemoteEncryptedIO : public EncryptedIO
{
    Enclave &m_enclave;

public:
    RemoteEncryptedIO(Enclave &enclave, const sgx_aes_gcm_128bit_key_t &disk_key);
    [[nodiscard]] bool read_from_disk(const std::string &filename, bitstream &data) override;
    [[nodiscard]] bool write_to_disk(const std::string &filename, const bitstream &data) override;
};

} // namespace trusted
} // namespace credb
