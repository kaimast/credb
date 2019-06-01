/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "PeerAcceptor.h"
#include "PeerHandler.h"
#include "util/defines.h"

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

#include <yael/EventLoop.h>
#include <yael/network/TcpSocket.h>
using namespace yael;

namespace credb::untrusted
{

static PeerAcceptor *g_peer_acceptor = nullptr;

PeerAcceptor::PeerAcceptor(EnclaveHandle &enclave, RemoteParties &remote_parties)
: m_enclave(enclave), m_remote_parties(remote_parties)
{
    g_peer_acceptor = this;
}

bool PeerAcceptor::init(uint16_t port)
{
    const std::string host = "0.0.0.0";
    auto socket = std::make_unique<network::TcpSocket>();
    bool res = socket->listen(host, port, 100);

    if(!res)
    {
        return false;
    }

    set_socket(std::move(socket), yael::SocketType::Acceptor);

    LOG(INFO) << "Listening for peers on host " << host << " port " << port;
    return true;
}

void PeerAcceptor::on_new_connection(std::unique_ptr<yael::network::Socket> &&socket)
{
    auto &el = EventLoop::get_instance();
    auto p =
    el.make_event_listener<PeerHandler>(m_enclave,
                                         std::forward<std::unique_ptr<yael::network::Socket>>(socket));
    auto id = m_remote_parties.register_remote_party(p);
    p->setup(id, false);
    p->unlock();
}

std::shared_ptr<PeerHandler> PeerAcceptor::get(remote_party_id id)
{
    auto party = m_remote_parties.get(id, true);
    return std::dynamic_pointer_cast<PeerHandler>(party);
}

remote_party_id PeerAcceptor::connect(const std::string &url)
{
    // FIXME check if we are already connected to this URL

    auto &el = EventLoop::get_instance();
    auto peer = el.make_event_listener<PeerHandler>(m_enclave, url);
    auto id = m_remote_parties.register_remote_party(peer);
    peer->setup(id, true);
    peer->unlock();

    // This will block until peer is connected
    peer->wait_for_connection();

    return peer->identifier();
}

} // namespace credb::untrusted
