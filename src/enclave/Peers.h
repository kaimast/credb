#pragma once

#include <unordered_map>
#include <unordered_set>

#include "util/Mutex.h"
#include "Peer.h"
#include "PendingBooleanResponse.h"
#include "RemoteParties.h"
#include "Task.h"

namespace credb
{
namespace trusted
{

class Enclave;

class Peers
{
public:
    class iterator
    {
    public:
        iterator(RemoteParties &remote_parties, std::unordered_map<std::string, remote_party_id> &peers)
        : m_remote_parties(remote_parties), m_peers(peers)
        {
            m_it = m_peers.begin();
        }

        uint32_t size() const { return m_peers.size(); }

        Peer &next()
        {
            auto id = (m_it++)->second;
            auto party = m_remote_parties.find(id);

            return dynamic_cast<Peer &>(*party);
        }

        bool has_next() { return m_it != m_peers.end(); }

    private:
        RemoteParties &m_remote_parties;
        std::unordered_map<std::string, remote_party_id> &m_peers;
        std::unordered_map<std::string, remote_party_id>::iterator m_it;
    };

    Peers(Enclave &enclave);

    void init_peer(remote_party_id id, const std::string &hostname, uint16_t port, bool is_initiator);

    bool handle_peer_message(remote_party_id identifier, const uint8_t *data, size_t len);

    iterator iterate() { return iterator(m_remote_parties, m_peers); }

    std::shared_ptr<Peer> find(remote_party_id id)
    {
        auto party = m_remote_parties.find(id);
        return std::dynamic_pointer_cast<Peer>(party);
    }

    std::shared_ptr<Peer> find_by_name(const std::string &name)
    {
        auto it = m_peers.find(name);

        if(it == m_peers.end())
            return nullptr;

        return find(it->second);
    }

    void handle_peer_disconnect(const remote_party_id id)
    {
        std::lock_guard lock(m_mutex);
        auto peer = find(id);

        auto it = m_peers.find(peer->name());

        if(it != m_peers.end())
        {
            m_peers.erase(it);
        }
    }

    void set_name(const remote_party_id id, const std::string &name) { m_peers[name] = id; }

    void remove(remote_party_id id);

    const std::unordered_set<remote_party_id> &get_downstream_set() const;
    void add_downstream_server(remote_party_id downstream_id);
    bool remove_downstream_server(remote_party_id downstream_id);

private:
    credb::Mutex m_mutex;
    Enclave &m_enclave;
    RemoteParties &m_remote_parties;
    std::unordered_map<std::string, remote_party_id> m_peers;
    std::unordered_set<remote_party_id> m_downstream_set;
};

} // namespace trusted
} // namespace credb
