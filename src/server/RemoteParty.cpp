#include "RemoteParty.h"

#include "RemoteParties.h"

namespace credb
{
namespace untrusted
{

RemoteParty::RemoteParty(std::unique_ptr<yael::network::Socket> &&socket, EnclaveHandle &enclave, AttestationMode att_mode)
: yael::NetworkSocketListener(std::forward<std::unique_ptr<yael::network::Socket>>(socket),
                              yael::SocketType::Connection),
  m_attestation(att_mode, *this, enclave)
{
    lock();
}

void RemoteParty::wait() { m_condition_var.wait(mutex()); }

void RemoteParty::notify_all() { m_condition_var.notify_all(); }

} // namespace untrusted
} // namespace credb
