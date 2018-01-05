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
#include "Peers.h"

namespace credb
{
namespace trusted
{

Peers::Peers(Enclave &enclave) : m_enclave(enclave), m_remote_parties(enclave.remote_parties()) {}

bool Peers::handle_peer_message(remote_party_id identifier, const uint8_t *data, size_t len)
{
    auto peer = find(identifier);

    if(!peer)
    {
        log_error("No such peer!");
        return false;
    }

    peer->handle_message(data, len);
    return true;
}

void Peers::remove(remote_party_id id)
{
    auto peer = find(id);
    if(!peer)
    {
        return;
    }

    if(peer->get_peer_type() == PeerType::DownstreamServer)
    {
        remove_downstream_server(id);
    }

    m_remote_parties.remove(id);
}

void Peers::init_peer(remote_party_id id, const std::string &hostname, uint16_t port, bool is_initiator)
{
    auto p = std::make_shared<Peer>(m_enclave, id, hostname, port, is_initiator);
    m_remote_parties.insert(id, p);
}

const std::unordered_set<remote_party_id> &Peers::get_downstream_set() const
{
    return m_downstream_set;
}

void Peers::add_downstream_server(remote_party_id downstream_id)
{
    log_debug("add downstream server " + std::to_string(downstream_id));
    m_downstream_set.insert(downstream_id);
}

bool Peers::remove_downstream_server(remote_party_id downstream_id)
{
    log_debug("remove downstream server " + std::to_string(downstream_id));
    return m_downstream_set.erase(downstream_id);
}

} // namespace trusted
} // namespace credb

#ifndef TEST

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

credb_status_t credb_handle_peer_disconnect(remote_party_id id)
{
    credb::trusted::g_enclave->peers().remove(id);
    return CREDB_SUCCESS;
}

credb_status_t credb_init_peer(remote_party_id id, const char *ip, uint16_t port, bool initiator)
{
    credb::trusted::g_enclave->peers().init_peer(id, ip, port, initiator);
    return CREDB_SUCCESS;
}

credb_status_t credb_handle_peer_message(remote_party_id identifier, const uint8_t *in_data, size_t in_len)
{
    auto res = credb::trusted::g_enclave->peers().handle_peer_message(identifier, in_data, in_len);

    if(res)
    {
        return CREDB_SUCCESS;
    }
    else
    {
        return CREDB_ERROR_UNEXPECTED;
    }
}

#endif
