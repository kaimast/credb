/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "Attestation.h"
#include "EnclaveHandle.h"
#include "RemoteParties.h"
#include "RemoteParty.h"

#include "util/EncryptionType.h"

namespace credb
{
namespace untrusted
{

class ClientAcceptor;
class PeerAcceptor;

class ClientHandler : public RemoteParty
{
public:
    ClientHandler(ClientAcceptor &acceptor,
                  std::unique_ptr<yael::network::Socket> &&socket,
                  EnclaveHandle &enclave,
                  PeerAcceptor &peer_acceptor);

    ClientHandler(const ClientHandler &other) = delete;

    ~ClientHandler() = default;

    void setup(remote_party_id identifier);

    std::string name() const override
    {
        return "Client #" + std::to_string(identifier());
    }

protected:
    void handle_plain_text_message(bitstream &msg);

    void on_network_message(yael::network::Socket::message_in_t &msg) override;

private:
    void init(bitstream &input);

    ClientAcceptor &m_acceptor;
    PeerAcceptor &m_peer_acceptor;
};

} // namespace untrusted
} // namespace credb
