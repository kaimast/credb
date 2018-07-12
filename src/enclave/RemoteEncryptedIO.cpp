#include "RemoteEncryptedIO.h"
#include "logging.h"

namespace credb
{
namespace trusted
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

} // namespace trusted
} // namespace credb
