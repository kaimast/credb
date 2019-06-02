/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once
#include "Enclave.h"
#include "EncryptedIO.h"

namespace credb::trusted
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

    bool is_remote() const override
    {
        return true;
    }

private:
    Enclave &m_enclave;
};

} // namespace credb::trusted
