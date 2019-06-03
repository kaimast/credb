/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#endif

#include "util/MessageType.h"
#include "util/OperationType.h"

#include "logging.h"

#include <cstring>
#include <map>

#include "Enclave.h"
#include "Ledger.h"
#include "RemoteParties.h"

namespace credb::trusted
{

RemoteParties::RemoteParties(Enclave &enclave) : m_enclave(enclave)
{}

std::vector<std::tuple<std::string, std::string, uint16_t>> RemoteParties::get_peer_infos()
{
    ReadLock lock(m_lockable);
    std::vector<std::tuple<std::string, std::string, uint16_t>> result;

    for(auto &[id, rp]: m_remote_parties)
    {
        (void)id;

        if(!rp->is_peer())
        {
            continue;
        }

        auto peer = std::dynamic_pointer_cast<Peer>(rp);
        result.emplace_back(std::make_tuple(peer->name(), peer->hostname(), peer->port()));
    }

    return result;
}

bool RemoteParties::handle_message(remote_party_id identifier, const uint8_t *data, size_t len)
{
    auto rp = find<RemoteParty>(identifier);

    if(!rp)
    {
        log_error("No such peer!");
        return false;
    }

    rp->handle_message(data, len);
    return true;
}

void RemoteParties::init_peer(remote_party_id id, const std::string &hostname, uint16_t port, bool is_initiator)
{
    WriteLock lock(m_lockable);

    auto p = std::make_shared<Peer>(m_enclave, id, hostname, port, is_initiator);
    auto res = m_remote_parties.insert({id, std::dynamic_pointer_cast<RemoteParty>(p)});

    if(!res.second)
    {
        log_error("Failed to create client. Already registered");
    }
}

void RemoteParties::init_client(remote_party_id id)
{
    WriteLock lock(m_lockable);

    auto c = std::make_shared<Client>(m_enclave, id);
    auto res = m_remote_parties.insert({id, std::dynamic_pointer_cast<RemoteParty>(c)});

    if(!res.second)
    {
        log_error("Failed to create client. Already registered");
    }
}

const std::unordered_set<remote_party_id> &RemoteParties::get_downstream_set() const
{
    //FIXME this is iffy, we should probably return a copy? 
    return m_downstream_set;
}

void RemoteParties::handle_disconnect(const remote_party_id local_id)
{
    WriteLock lock(m_lockable);

    auto it = m_remote_parties.find(local_id);

    if(it == m_remote_parties.end())
    {
        log_error("Cannot handle disconnect: no such remote party");
        return; 
    }

    auto rp = it->second;

    if(rp->has_identity())
    {
        auto it1 = m_name_mapping.find(rp->name());

        if(it1 != m_name_mapping.end() && it1->second == local_id)
        {
            m_name_mapping.erase(it1);
        }

        auto it2 = m_uid_mapping.find(rp->identity().get_unique_id());

        if(it2 != m_uid_mapping.end() && it2->second == local_id)
        {
            m_uid_mapping.erase(it2);
        }
    }

    if(rp->is_peer())
    {
        auto peer = std::dynamic_pointer_cast<Peer>(rp);
        if(peer->get_peer_type() == PeerType::DownstreamServer)
        {
            log_debug("removing downstream server " + std::to_string(local_id));
            m_downstream_set.erase(local_id);
        }
    }

    m_remote_parties.erase(it);
}

void RemoteParties::add_downstream_server(remote_party_id downstream_id)
{
    WriteLock lock(m_lockable);

    log_debug("add downstream server " + std::to_string(downstream_id));
    m_downstream_set.insert(downstream_id);
}

void RemoteParties::set_identity(remote_party_id local_id, const Identity &identity)
{
    std::shared_ptr<RemoteParty> to_disconnect = nullptr;
    
    {
        WriteLock lock(m_lockable);

        auto uid = identity.get_unique_id();
        auto it = m_uid_mapping.find(uid);

        if(it != m_uid_mapping.end())
        {
            auto other = find_internal<RemoteParty>(it->second);

            if(other->name() != identity.name())
            {
                log_warning("Already connected to a party with the same key as \"" + identity.name() + "\", named \"" + other->name() + "\". Marking old one to be removed.");
            }
            else
            {
                log_warning("Already connected to identity \"" + identity.name() + "\". Marking old one to be removed.");
            }

            to_disconnect = other;
        }

        m_uid_mapping[uid] = local_id;
        m_name_mapping[identity.name()] = local_id;
    }

    if(to_disconnect != nullptr)
    {
        to_disconnect->lock();
        to_disconnect->disconnect();
        to_disconnect->unlock();
    }
}

} // namespace credb::trusted

#define MAC_KEY_SIZE 16
int memcpy_s(

void *dest,
size_t numberOfElements,
const void *src,
size_t count)
{
    if(numberOfElements < count)
    {
        return -1;
    }

    memcpy(dest, src, count);
    return 0;
}

credb_status_t credb_handle_disconnect(remote_party_id identifier)
{
    credb::trusted::g_enclave->remote_parties().handle_disconnect(identifier);
    return CREDB_SUCCESS;
}

credb_status_t credb_init_client(remote_party_id identifier)
{
    credb::trusted::g_enclave->remote_parties().init_client(identifier);
    return CREDB_SUCCESS;
}

credb_status_t credb_init_peer(remote_party_id id, const char *ip, uint16_t port, bool initiator)
{
    credb::trusted::g_enclave->remote_parties().init_peer(id, ip, port, initiator);
    return CREDB_SUCCESS;
}

credb_status_t credb_handle_message(remote_party_id identifier, const uint8_t *in_data, size_t in_len)
{
    auto res = credb::trusted::g_enclave->remote_parties().handle_message(identifier, in_data, in_len);

    if(res)
    {
        return CREDB_SUCCESS;
    }
    else
    {
        return CREDB_ERROR_UNEXPECTED;
    }
}

/// Enclave API
void credb_set_attestation_context(remote_party_id identifier, sgx_ra_context_t context)
{
    auto remote_party = credb::trusted::g_enclave->remote_parties().find<credb::trusted::RemoteParty>(identifier);

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
    auto party = credb::trusted::g_enclave->remote_parties().find<credb::trusted::RemoteParty>(id);

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
        log_error(to_string(res));
        return CREDB_ERROR_UNEXPECTED;
    }
#endif
}
