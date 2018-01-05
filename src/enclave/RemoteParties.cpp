#include "RemoteParties.h"
#include "Enclave.h"
#include "RemoteParty.h"
#include "logging.h"

#include <sgx_tkey_exchange.h>

namespace credb
{
namespace trusted
{

RemoteParties::RemoteParties() = default;

void RemoteParties::insert(remote_party_id key, std::shared_ptr<RemoteParty> party)
{
    std::lock_guard lock(m_mutex);
    m_remote_parties.insert({ key, party });
}

void RemoteParties::remove(remote_party_id identifier)
{
    std::lock_guard lock(m_mutex);
    
    auto it = m_remote_parties.find(identifier);
    if(it == m_remote_parties.end())
    {
        log_error("No such remote party");
    }
    else
    {
        m_remote_parties.erase(it);
    }
}

std::shared_ptr<RemoteParty> RemoteParties::find(remote_party_id identifier)
{
    std::lock_guard lock(m_mutex);
    auto it = m_remote_parties.find(identifier);

    if(it == m_remote_parties.end())
    {
        log_debug("No such remote party");
        return nullptr;
    }

    return it->second;
}

} // namespace trusted
} // namespace credb

#ifndef TEST

/// Enclave API
void credb_set_attestation_context(remote_party_id identifier, sgx_ra_context_t context)
{
    auto remote_party = credb::trusted::g_enclave->remote_parties().find(identifier);

    if(remote_party)
    {
        remote_party->set_attestation_context(context);
    }
    else
    {
        log_debug("No such remote party");
    }
}

credb_status_t credb_ra_init(remote_party_id id, sgx_ra_context_t* context)
{
    auto party = credb::trusted::g_enclave->remote_parties().find(id);

    if(!party)
    {
        return CREDB_ERROR_INVALID_PARAMETER;
    }

#ifdef FAKE_ENCLAVE
    (void)id;
    memset(context, 0, sizeof(*context)); //clang-tidy
    return CREDB_SUCCESS;
#else
    auto res = sgx_ra_init(&party->public_key(), false, context);

    if(res == SGX_SUCCESS)
    {
        return CREDB_SUCCESS;
    }
    else
    {
        return CREDB_ERROR_UNEXPECTED;
    }
#endif
}

#endif
