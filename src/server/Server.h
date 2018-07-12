#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <yael/EventListener.h>
#include <bitstream.h>

#include "../error.h"
#include "Attestation.h"
#include "EnclaveHandle.h"
#include "Disk.h"
#include "ClientAcceptor.h"
#include "PeerAcceptor.h"

namespace credb
{
namespace untrusted
{

class Server
{
public:
    Server(const std::string &name, const std::string &addr, uint16_t port, const std::string &disk_path);
    ~Server();

    void listen(uint16_t port);
    remote_party_id connect(const std::string &addr);
    void set_upstream(const std::string &addr);

private:
    Disk m_disk;

    RemoteParties m_remote_parties;
    EnclaveHandle m_enclave;

    std::shared_ptr<ClientAcceptor> m_client_acceptor = nullptr;
    std::shared_ptr<PeerAcceptor> m_peer_acceptor = nullptr;
};

} // namespace untrusted
} // namespace credb
