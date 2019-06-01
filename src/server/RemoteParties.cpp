/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "RemoteParties.h"
#include "PeerHandler.h"

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

namespace credb::untrusted
{

RemoteParties *g_remote_parties = nullptr;

RemoteParties::RemoteParties() : m_next_remote_party_id(1) { g_remote_parties = this; }

remote_party_id RemoteParties::register_remote_party(std::shared_ptr<RemoteParty> party)
{
    WriteLock lock(m_lockable);

    auto id = m_next_remote_party_id;
    m_next_remote_party_id++;
    m_remote_parties.insert({ id, party });

    return id;
}

} // namespace credb::untrusted

void attestation_queue_groupid_result(remote_party_id identifier, bool result)
{
    auto b = new bool(result);

    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->get_attestation().queue_groupid_result(b);
}

void attestation_queue_msg2(remote_party_id identifier, sgx_ra_msg2_t *p_msg2, uint32_t size)
{
    auto msg2 = reinterpret_cast<sgx_ra_msg2_t *>(malloc(size));
    memcpy(msg2, p_msg2, size);

    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->get_attestation().queue_msg2(msg2, size);
}

void attestation_notify_done(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->get_attestation().set_done();
}

void attestation_tell_groupid(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->get_attestation().tell_groupid();
}

void disconnect_remote_party(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->disconnect();
}

void send_to_remote_party(remote_party_id identifier, const uint8_t *data, uint32_t length)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->send(data, length);
}

void remote_party_lock(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->lock();
}

void remote_party_unlock(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->unlock();
}

void remote_party_wait(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->wait();
}

void remote_party_notify_all(remote_party_id identifier)
{
    auto rp = credb::untrusted::g_remote_parties->get(identifier, false);
    rp->notify_all();
}
