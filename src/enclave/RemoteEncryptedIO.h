#pragma once
#include "Enclave.h"
#include "EncryptedIO.h"

namespace credb
{
namespace trusted
{

class RemoteEncryptedIO : public EncryptedIO
{
public:
    RemoteEncryptedIO(Enclave &enclave, const sgx_aes_gcm_128bit_key_t &disk_key);
    [[nodiscard]] bool read_from_disk(const std::string &filename, bitstream &data) override;
    [[nodiscard]] bool write_to_disk(const std::string &filename, const bitstream &data) override;

    size_t num_files() override
    {
        //Not supported
        return 0;
    }

    size_t total_file_size() override
    {
        //Not supported
        return 0;
    }

    bool is_remote() const
    {
        return true;
    }

private:
    Enclave &m_enclave;
};

} // namespace trusted
} // namespace credb
