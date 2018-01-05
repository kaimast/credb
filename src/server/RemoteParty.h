#pragma once

#include <bitstream.h>
#include <mutex>
#include <sgx_ukey_exchange.h>
#include <yael/NetworkSocketListener.h>
#include <yael/network/Socket.h>

#include "Attestation.h"
#include "util/defines.h"

namespace credb
{
namespace untrusted
{

class RemoteParties;

class RemoteParty : public yael::NetworkSocketListener
{
public:
    remote_party_id identifier() const { return m_identifier; }

    virtual void send(const uint8_t *data, uint32_t len) = 0;

    inline void send(const bitstream &data) { send(data.data(), data.size()); }

    Attestation &get_attestation() { return m_attestation; }

    virtual void disconnect() = 0;

    virtual void set_attestation_context(sgx_ra_context_t context) = 0;

    virtual ~RemoteParty() {}

    void wait();

    void notify_all();

    void set_identifier(remote_party_id id)
    {
        if(m_identifier != INVALID_REMOTE_PARTY || id == INVALID_REMOTE_PARTY)
            throw std::runtime_error("set_identifier() error");

        m_identifier = id;
    }

protected:
    // Must be called by child class
    // Child class must call unlock() after setup is complete
    RemoteParty(std::unique_ptr<yael::network::Socket> &&socket, EnclaveHandle &enclave, AttestationMode att_mode);

private:
    Attestation m_attestation;

    remote_party_id m_identifier = INVALID_REMOTE_PARTY;

    std::condition_variable_any m_condition_var;
};

} // namespace untrusted
} // namespace credb
