/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "ClientAcceptor.h"
#include "ClientHandler.h"
#include "util/defines.h"
#include <yael/network/TcpSocket.h>

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

#include <yael/EventLoop.h>
using namespace yael;

namespace credb::untrusted
{

static ClientAcceptor *g_client_acceptor = nullptr;

ClientAcceptor::ClientAcceptor(EnclaveHandle &enclave,
                               untrusted::RemoteParties &remote_parties,
                               PeerAcceptor &peer_acceptor,
                               const std::string &addr,
                               uint16_t port)
: m_enclave(enclave), m_remote_parties(remote_parties), m_peer_acceptor(peer_acceptor)
{
    if(port == 0)
    {
        port = CLIENT_PORT;
    }

    auto socket = std::make_unique<yael::network::TcpSocket>();
    bool res = socket->listen(addr, port, 100);

    if(!res)
    {
        LOG(FATAL) << "Could not list on port " << port;
    }

    this->set_socket(std::move(socket), yael::SocketType::Acceptor);

    g_client_acceptor = this;
    LOG(INFO) << "Listening for clients on host " << addr << " port " << port;
}

void ClientAcceptor::on_new_connection(std::unique_ptr<yael::network::Socket> &&socket)
{
    auto &el = EventLoop::get_instance();
    auto c =
    el.make_event_listener<ClientHandler>(*this, std::forward<std::unique_ptr<yael::network::Socket>>(socket),
                                           m_enclave, m_peer_acceptor);
    auto id = m_remote_parties.register_remote_party(c);
    c->setup(id);
    c->unlock();
}

std::shared_ptr<ClientHandler> ClientAcceptor::get(remote_party_id identifier)
{
    auto party = m_remote_parties.get(identifier, true);
    return std::dynamic_pointer_cast<ClientHandler>(party);
}

} // namespace credb::untrusted
