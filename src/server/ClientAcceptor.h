/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "ClientHandler.h"
#include "EnclaveHandle.h"
#include "PeerAcceptor.h"
#include "RemoteParties.h"
#include <unordered_map>
#include <yael/NetworkSocketListener.h>

namespace credb
{
namespace untrusted
{

class ClientAcceptor : public yael::NetworkSocketListener
{
public:
    ClientAcceptor(EnclaveHandle &enclave,
                   untrusted::RemoteParties &remote_parties,
                   PeerAcceptor &peer_acceptor,
                   const std::string &addr,
                   uint16_t port);
    ClientAcceptor(const ClientAcceptor &other) = delete;

    std::shared_ptr<ClientHandler> get(remote_party_id identifier);

protected:
    void on_new_connection(std::unique_ptr<yael::network::Socket> &&socket) override;

private:
    EnclaveHandle &m_enclave;
    RemoteParties &m_remote_parties;
    PeerAcceptor &m_peer_acceptor;
};

} // namespace untrusted
} // namespace credb
