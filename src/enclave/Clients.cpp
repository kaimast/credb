#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <string>

#include "Client.h"
#include "Clients.h"
#include "Enclave.h"
#include "Ledger.h"
#include "logging.h"

#include "RemoteParties.h"

namespace credb
{
namespace trusted
{

Clients::Clients(Enclave &enclave)
: m_enclave(enclave), m_remote_parties(m_enclave.remote_parties())
{
}

void Clients::init_client(int32_t id)
{
    if(id <= 0)
    {
        log_error("invalid client id");
        return;
    }

    auto c = std::make_shared<Client>(m_enclave, id);
    m_remote_parties.insert(id, c);
}

std::shared_ptr<Client> Clients::find(remote_party_id id)
{
    auto party = m_remote_parties.find(id);
    return std::dynamic_pointer_cast<Client>(party);
}

credb_status_t Clients::handle_client_message(remote_party_id id, const uint8_t *data, uint32_t length)
{
    auto client = find(id);
    client->handle_message(data, length);
    return CREDB_SUCCESS;
}

void Clients::set_attestation_context(remote_party_id id, sgx_ra_context_t context)
{
    auto client = find(id);
    client->set_attestation_context(context);
}

void Clients::handle_client_disconnect(remote_party_id id) { m_remote_parties.remove(id); }

} // namespace trusted
} // namespace credb

#ifndef TEST

void credb_init_client(remote_party_id identifier) { credb::trusted::g_enclave->clients().init_client(identifier); }

credb_status_t credb_handle_client_message(remote_party_id identifier, const uint8_t *in_data, size_t in_len)
{
    return credb::trusted::g_enclave->clients().handle_client_message(identifier, in_data, in_len);
}

void credb_handle_client_disconnect(remote_party_id identifier)
{
    credb::trusted::g_enclave->clients().handle_client_disconnect(identifier);
}
#endif
