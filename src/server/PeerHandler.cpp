/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Disk.h"
#include "EnclaveHandle.h"
#include "PeerAcceptor.h"
#include "PeerHandler.h"

#include "util/error.h"
#include "util/defines.h"
#include "util/EncryptionType.h"
#include "util/MessageType.h"
#include "util/OperationType.h"

#include <yael/network/TcpSocket.h>
#include <stdbitstream.h>

using namespace yael;

namespace credb::untrusted
{

PeerHandler::PeerHandler(EnclaveHandle &enclave, std::unique_ptr<yael::network::Socket> &&s)
    : RemoteParty(std::forward<std::unique_ptr<yael::network::Socket>>(s), enclave, AttestationMode::TwoWay_NotInitiator)
{
    LOG(INFO) << "New peer connected";
}

PeerHandler::PeerHandler(EnclaveHandle &enclave, const std::string &url)
    : RemoteParty(nullptr, enclave, AttestationMode::TwoWay_IsInitiator)
{
    int port;
    std::string host;
    std::string::size_type pos = url.rfind(':');
    if (pos != std::string::npos) // Contains port and addr
    {
        port = std::stoi(url.substr(pos+1));
        host = url.substr(0, pos);
    }
    else
    {
        port = SERVER_PORT;
        host = url;
    }

    LOG(INFO) << "Connecting to host " << host << " port " << port;

    auto sock = std::make_unique<network::TcpSocket>();
    auto addr = network::resolve_URL(host, port);
    bool success = sock->connect(addr);

    if(!success)
    {
        throw std::runtime_error("Failed to connect to other server " + host + ":" + std::to_string(port));
    }

    NetworkSocketListener::set_socket(std::move(sock), SocketType::Connection);
}

PeerHandler::~PeerHandler()
{
    LOG(INFO) << "PeerHandler " << identifier() << " destructing"; // FIXME: should destruct when disconnect
}

void PeerHandler::setup(remote_party_id id, bool is_initiator)
{
    set_identifier(id);
    m_enclave.init_peer(identifier(), hostname(), port(), is_initiator);
    get_attestation().init();
}

void PeerHandler::on_network_message(yael::network::message_in_t &msg)
{
    bitstream peeker;
    peeker.assign(msg.data, msg.length, true);
    
    EncryptionType encryption;
    peeker >> reinterpret_cast<etype_data_t&>(encryption);

    switch (encryption)
    {
    case EncryptionType::PlainText:
        handle_peer_plain_text_message(msg.data, msg.length);
        break;
    case EncryptionType::Encrypted:
    case EncryptionType::Attestation:
        m_enclave.handle_message(identifier(), msg.data, msg.length);
        break;
    default:
        LOG(ERROR) << "Unknown encryption header field: " << static_cast<int>(encryption);
    }

    get_attestation().update();
}

void PeerHandler::handle_peer_plain_text_message(const uint8_t *data, uint32_t len)
{
    // DLOG(INFO) << "Received plain text message from peer " << identifier();
    bitstream input(data, len);

    EncryptionType encryption;
    input >> reinterpret_cast<etype_data_t&>(encryption);
    if (encryption != EncryptionType::PlainText)
    {
        LOG(ERROR) << "The message is not a plain text";
        return;
    }

    MessageType type;
    input >> reinterpret_cast<mtype_data_t&>(type);
    switch (type) {
    case MessageType::OperationRequest: handle_op_request(input); break;
    case MessageType::OperationResponse: handle_op_response(input); break;
    default: LOG(ERROR) << "Received unknown request MessageType=" << static_cast<uint8_t>(type) << " from peer " << identifier();
    }
}

void PeerHandler::handle_op_request(bitstream &input)
{
    unlock();
    OperationType op_type;
    operation_id_t op_id;
    input >> op_id;
    input >> reinterpret_cast<op_data_t&>(op_type);

    bitstream output;
    output << EncryptionType::PlainText;
    output << MessageType::OperationResponse;
    output << op_id;

    switch(op_type)
    {
    case OperationType::ReadFromUpstreamDisk:
    {
        std::string path;
        input >> path;
//        DLOG(INFO) << "handle_op_request, peer_id=" << identifier() << " op_id=" << op_id << " OperationType::ReadFromUpstreamDisk: " << path;

        bitstream bstream;
        bool ok = m_enclave.disk().read_undecrypted_from_disk(path, bstream);
        output << bstream;

        if(!ok)
        {
            LOG(ERROR) << "Failed to read_undecrypted_from_disk: " << path;
        }
        break;
    }
    default:
        LOG(ERROR) << "Received unknown op type " << static_cast<uint8_t>(op_type) << " from peer " << identifier();
    }

    lock();
    send(output);
//    DLOG(INFO) << "handle_op_request, peer_id=" << identifier() << " op_id=" << op_id << " done";
}

void PeerHandler::handle_op_response(bitstream &input)
{
    operation_id_t op_id;
    input >> op_id;
    // DLOG(INFO) << "Received operation response, peer_id=" << identifier() << ", op_id=" << op_id;

    uint32_t pos = input.pos();
    g_enclave_handle->peer_insert_response(identifier(), op_id, input.data() + pos, input.size() - pos);
    this->notify_all();
}

} // namespace credb::untrusted
