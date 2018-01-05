#pragma once

#include "util/Mutex.h"
#include <unordered_map>

#include "RemoteParty.h"

namespace credb
{
namespace untrusted
{

class RemoteParties
{
public:
    RemoteParties();

    void unregister_remote_party(remote_party_id identifier)
    {
        m_rp_mutex.lock();
        auto it = m_remote_parties.find(identifier);
        if(it == m_remote_parties.end())
            throw std::runtime_error("No such remote_party");

        m_remote_parties.erase(it);
        m_rp_mutex.unlock();
    }

    std::shared_ptr<RemoteParty> get(remote_party_id identifier, bool lock)
    {
        m_rp_mutex.lock();
        auto it = m_remote_parties.find(identifier);

        if(it == m_remote_parties.end())
            throw std::runtime_error("No such remote party!");

        auto p = it->second;
        if(lock)
            p->lock();
        m_rp_mutex.unlock();
        return p;
    }

    // Shall only be called by PeerAcceptor and ClientAcceptor
    remote_party_id register_remote_party(std::shared_ptr<RemoteParty> party);

private:
    remote_party_id m_next_remote_party_id;

    credb::Mutex m_rp_mutex;
    std::unordered_map<remote_party_id, std::shared_ptr<RemoteParty>> m_remote_parties;
};

} // namespace untrusted
} // namespace credb
