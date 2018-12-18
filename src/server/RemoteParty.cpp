/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "RemoteParty.h"

#include "RemoteParties.h"

namespace credb
{
namespace untrusted {

RemoteParty::RemoteParty(std::unique_ptr<yael::network::Socket> &&socket, EnclaveHandle &enclave, AttestationMode att_mode)
: yael::NetworkSocketListener(std::forward<std::unique_ptr<yael::network::Socket>>(socket),
                              yael::SocketType::Connection),
  m_enclave(enclave), m_attestation(att_mode, *this, enclave)
{
    lock();
}

void RemoteParty::on_disconnect()
{
    m_enclave.handle_disconnect(identifier());
    LOG(INFO) << name() << " disconnected";
}

void RemoteParty::send(const uint8_t *data, uint32_t length)
{
    try
    {
        NetworkSocketListener::send(data, length);
    }
    catch(yael::network::socket_error &e)
    {
        LOG(ERROR) << e.what();
    }
}

void RemoteParty::set_attestation_context(sgx_ra_context_t context)
{
    m_enclave.set_attestation_context(identifier(), context);
}

void RemoteParty::wait() { m_condition_var.wait(mutex()); }

void RemoteParty::notify_all() { m_condition_var.notify_all(); }

} // namespace untrusted
} // namespace credb
