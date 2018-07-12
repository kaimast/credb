#pragma once

#include <unordered_map>
#include <unordered_set>

#include "util/Mutex.h"
#include "Peer.h"
#include "Client.h"
#include "PendingResponse.h"
#include "Task.h"

namespace credb
{
namespace trusted
{

class Enclave;

/**
 * This class keeps track of all currently connected RemoteParties
 * RemoteParties are other CreDB nodes
 */
class RemoteParties
{
public:
    RemoteParties(Enclave &enclave);

    void init_peer(remote_party_id id, const std::string &hostname, uint16_t port, bool is_initiator);

    void init_client(remote_party_id id);
    
    bool handle_message(remote_party_id identifier, const uint8_t *data, size_t len);

    void set_identity(remote_party_id local_id, const Identity &identity);

    template<typename T>
    std::shared_ptr<T> find(remote_party_id id)
    {
        ReadLock lock(m_lockable);
        return find_internal<T>(id);
    }

    template<typename T>
    std::shared_ptr<T> find_by_uid(const identity_uid_t uid)
    {
        ReadLock lock(m_lockable);
        
        auto it = m_uid_mapping.find(uid);

        if(it == m_uid_mapping.end())
        {
            return nullptr;
        }

        return find_internal<T>(it->second);
    }

    template<typename T>
    std::shared_ptr<T> find_by_name(const std::string &name)
    {
        ReadLock lock(m_lockable);
 
        auto it = m_name_mapping.find(name);

        if(it == m_name_mapping.end())
        {
            return nullptr;
        }

        return find_internal<T>(it->second);
    }

    /**
     * Get a list of information about all peers
     *
     * Each info is a tuple of <name, hostname, port>
     */
    std::vector<std::tuple<std::string, std::string, uint16_t>> get_peer_infos();

    void handle_disconnect(remote_party_id local_id);

    const std::unordered_set<remote_party_id> &get_downstream_set() const;
    void add_downstream_server(remote_party_id downstream_id);

private:
    /**
     * Helper to retrieve a remote party
     *
     * Note, you must hold the lock before calling this function
     */
    template<typename T>
    std::shared_ptr<T> find_internal(remote_party_id id)
    {
        auto it = m_remote_parties.find(id);

        if(it == m_remote_parties.end())
        {
            return {nullptr};
        }

        return std::dynamic_pointer_cast<T>(it->second);
    }

    Enclave &m_enclave;

    RWLockable m_lockable;
    
    std::unordered_map<std::string, remote_party_id> m_name_mapping;
    std::unordered_map<identity_uid_t, remote_party_id> m_uid_mapping;
    std::unordered_set<remote_party_id> m_downstream_set;
    std::unordered_map<remote_party_id, std::shared_ptr<RemoteParty>> m_remote_parties;
};

} // namespace trusted
} // namespace credb
