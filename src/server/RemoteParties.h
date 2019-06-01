/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "util/Mutex.h"
#include "util/RWLockable.h"

#include "RemoteParty.h"

namespace credb::untrusted
{

/**
 * Manages all connected entities on the untrusted side
 *
 * Each remote party will be assigned a unique local id
 * such that no two parties ever have the same identifier
 */
class RemoteParties
{
public:
    RemoteParties();

    void unregister_remote_party(remote_party_id identifier)
    {
        WriteLock write_lock(m_lockable);
        
        auto it = m_remote_parties.find(identifier);
        if(it == m_remote_parties.end())
        {
            throw std::runtime_error("No such remote_party");
        }

        m_remote_parties.erase(it);
    }

    std::shared_ptr<RemoteParty> get(remote_party_id identifier, bool lock)
    {
        ReadLock read_lock(m_lockable);

        auto it = m_remote_parties.find(identifier);

        if(it == m_remote_parties.end())
        {
            throw std::runtime_error("No such remote party!");
        }

        auto p = it->second;
        if(lock)
        {
            p->lock();
        }
        
        return p;
    }

    // Shall only be called by PeerAcceptor and ClientAcceptor
    remote_party_id register_remote_party(std::shared_ptr<RemoteParty> party);

private:
    remote_party_id m_next_remote_party_id;

    RWLockable m_lockable;
    std::unordered_map<remote_party_id, std::shared_ptr<RemoteParty>> m_remote_parties;
};

} // namespace credb::untrusted
