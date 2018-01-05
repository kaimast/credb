#pragma once

#include "Client.h"
#include <map>

namespace credb
{
namespace trusted
{

class Enclave;
class RemoteParties;

class Clients
{
    Enclave &m_enclave;
    RemoteParties &m_remote_parties;

public:
    Clients(Enclave &enclave);

    void init_client(int32_t id);
    void handle_client_disconnect(remote_party_id id);

    void set_attestation_context(remote_party_id id, sgx_ra_context_t context);

    credb_status_t handle_client_message(remote_party_id id, const uint8_t *data, uint32_t length);

    std::shared_ptr<Client> find(remote_party_id id);
};

} // namespace trusted
} // namespace credb
