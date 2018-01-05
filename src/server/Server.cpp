#include "Server.h"

#include <glog/logging.h>
#include <iostream>

#include "Attestation.h"

#include <yael/EventLoop.h>
#include <yael/network/Socket.h>

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

#include "util/MessageType.h"
#include "util/defines.h"

using namespace yael;

namespace credb
{
namespace untrusted
{

Server::Server(const std::string &name, const std::string &addr, uint16_t port)
: m_enclave(name)
{
    auto &el = EventLoop::get_instance();

    m_peer_acceptor = el.allocate_socket_listener<PeerAcceptor>(m_enclave, m_remote_parties);
    m_client_acceptor =
    el.allocate_socket_listener<ClientAcceptor>(m_enclave, m_remote_parties, *m_peer_acceptor, addr, port);

    EventLoop::get_instance().register_socket_listener(m_client_acceptor);
}

Server::~Server() = default;

void Server::listen(uint16_t port)
{
    if(!m_peer_acceptor->init(port))
    {
        throw std::runtime_error("Failed to list on socket");
    }

    EventLoop::get_instance().register_socket_listener(m_peer_acceptor);
}

remote_party_id Server::connect(const std::string &addr)
{
    return m_peer_acceptor->connect(addr);
}

void Server::set_upstream(const std::string &addr)
{
    remote_party_id id = connect(addr);
    m_enclave.set_upstream(id);
}

} // namespace untrusted
} // namespace credb

/// GLOG bindings for the enclave

void print_info(const char *str) { LOG(INFO) << "ENCLAVE: " << str; }

void print_error(const char *str) { LOG(ERROR) << "ENCLAVE: " << str; }
