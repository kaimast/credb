#pragma once

#include <unordered_map>
#include <yael/NetworkSocketListener.h>

#include "EnclaveHandle.h"
#include "RemoteParties.h"

namespace credb
{
namespace untrusted
{

class PeerHandler;

class PeerAcceptor : public yael::NetworkSocketListener
{
public:
    PeerAcceptor(EnclaveHandle &enclave, untrusted::RemoteParties &remote_parties);

    bool init(uint16_t port);

    remote_party_id connect(const std::string &url);

    bool is_connected();

    std::shared_ptr<PeerHandler> get(remote_party_id id);

protected:
    void on_new_connection(std::unique_ptr<yael::network::Socket> &&socket) override;

private:
    EnclaveHandle &m_enclave;
    untrusted::RemoteParties &m_remote_parties;
};

} // namespace untrusted
} // namespace credb
