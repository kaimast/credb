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

    ~ClientHandler();

    void setup(remote_party_id identifier);

    void disconnect() override { socket().close(); }

    void set_attestation_context(sgx_ra_context_t context) override;

    using untrusted::RemoteParty::send;
    void send(const uint8_t *data, uint32_t length) override;

protected:
    void handle_plain_text_message(bitstream &msg);

    void on_network_message(yael::network::Socket::message_in_t &msg) override;
    void on_disconnect() override;

private:
    void init(bitstream &input);

    ClientAcceptor &m_acceptor;
    EnclaveHandle &m_enclave;
    PeerAcceptor &m_peer_acceptor;
};

} // namespace untrusted
} // namespace credb
