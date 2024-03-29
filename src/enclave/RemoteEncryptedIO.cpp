/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "RemoteEncryptedIO.h"
#include "logging.h"

namespace credb::trusted
{

RemoteEncryptedIO::RemoteEncryptedIO(Enclave &enclave, const sgx_aes_gcm_128bit_key_t &disk_key)
    : m_enclave(enclave)
{
    set_disk_key(disk_key);
}

// TODO: make downstream use local disk content as well

bool RemoteEncryptedIO::read_from_disk(const std::string &filename, bitstream &data)
{
    return m_enclave.read_from_upstream_disk(filename, data);
}

bool RemoteEncryptedIO::write_to_disk(const std::string &filename, const bitstream &data)
{
    // Writes area handled by upstream;
    (void)filename;
    (void)data;
    return true;
}

} // namespace credb::trusted
