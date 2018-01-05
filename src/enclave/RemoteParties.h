#pragma once

#include "RemoteParty.h"
#include "util/Mutex.h"
#include <unordered_map>

namespace credb
{
namespace trusted
{

class Enclave;

class RemoteParties
{
public:
    RemoteParties();

    void insert(remote_party_id key, std::shared_ptr<RemoteParty> party);

    void remove(remote_party_id identifier);

    std::shared_ptr<RemoteParty> find(remote_party_id identifier);

private:
    credb::Mutex m_mutex;
    std::unordered_map<remote_party_id, std::shared_ptr<RemoteParty>> m_remote_parties;
};

} // namespace trusted
} // namespace credb
