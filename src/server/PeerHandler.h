#pragma once

#include <condition_variable>

#include "util/MessageType.h"

#include "Attestation.h"
#include "EnclaveHandle.h"
#include "RemoteParties.h"
#include "RemoteParty.h"

namespace credb
{
namespace untrusted
{

class PeerAcceptor;

class PeerHandler : public untrusted::RemoteParty
{
public:
    PeerHandler(EnclaveHandle &enclave, std::unique_ptr<yael::network::Socket> &&s);
    PeerHandler(EnclaveHandle &enclave, const std::string &url);

    ~PeerHandler();

    void setup(remote_party_id id, bool is_initiator);

    bool is_connected() const { return m_connected; }

    void wait_for_connection() { get_attestation().wait_for_attestation(); }

    void handle_peer_plain_text_message(const uint8_t *data, uint32_t len);
    void set_attestation_context(sgx_ra_context_t context) override;

    using RemoteParty::send;
    void send(const uint8_t *data, uint32_t length) override;

    std::string hostname() const { return socket().get_client_address().IP; }

    void disconnect() override { socket().close(); }

    uint16_t port() const { return socket().get_client_address().PortNumber; }

protected:
    void on_network_message(yael::network::Socket::message_in_t &msg) override;
    void on_disconnect() override;

private:
    void handle_op_request(bitstream &input);
    void handle_op_response(bitstream &input);

#ifndef FAKE_ENCLAVE
//    uint32_t m_client_groupid = 0;
#endif

    EnclaveHandle &m_enclave;

    bool m_connected;
};

} // namespace untrusted
} // namespace credb
