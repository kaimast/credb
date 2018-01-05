#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

#include "EnclaveHandle.h"

#include "../error.h"

#include <bitstream.h>

#include "Attestation.h"

#include <yael/SocketListener.h>

#include "ClientAcceptor.h"
#include "PeerAcceptor.h"

namespace credb
{
namespace untrusted
{

class Server
{
public:
    Server(const std::string &name, const std::string &addr, uint16_t port);
    ~Server();

    void listen(uint16_t port);
    remote_party_id connect(const std::string &addr);
    void set_upstream(const std::string &addr);

private:
    RemoteParties m_remote_parties;
    EnclaveHandle m_enclave;

    std::shared_ptr<ClientAcceptor> m_client_acceptor = nullptr;
    std::shared_ptr<PeerAcceptor> m_peer_acceptor = nullptr;
};

} // namespace untrusted
} // namespace credb
