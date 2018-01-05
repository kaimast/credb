#include "EnclaveHandle.h"
#include "PeerHandler.h"
#include "Disk.h"
#include "util/MessageType.h"
#include "util/OperationType.h"
#include "util/EncryptionType.h"
#include "util/error.h"
#include "util/defines.h"
#include "PeerAcceptor.h"

using namespace yael;

namespace credb
{
namespace untrusted
{

PeerHandler::PeerHandler(EnclaveHandle &enclave, std::unique_ptr<yael::network::Socket> &&s)
    : RemoteParty(std::forward<std::unique_ptr<yael::network::Socket>>(s), enclave, AttestationMode::TwoWay_NotInitiator), m_enclave(enclave)
{
    LOG(INFO) << "New peer connected";
}

PeerHandler::PeerHandler(EnclaveHandle &enclave, const std::string &url)
    : RemoteParty(nullptr, enclave, AttestationMode::TwoWay_IsInitiator), m_enclave(enclave)
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

    auto sock = std::make_unique<network::Socket>();
    auto addr = network::resolve_URL(host, port);
    bool success = sock->connect(addr);

    if(!success)
    {
      throw std::runtime_error("Failed to connect to other server");
    }

    NetworkSocketListener::set_socket(std::move(sock), SocketType::Connection);
}

PeerHandler::~PeerHandler()
{
    LOG(INFO) << "PeerHandler " << identifier() << " destructing"; // FIXME: should destruct when disconnect
}

void PeerHandler::on_disconnect()
{
    m_enclave.handle_peer_disconnect(identifier());
    LOG(INFO) << "Peer disconnected";
}

void PeerHandler::setup(remote_party_id id, bool is_initiator)
{
    set_identifier(id);
    m_enclave.init_peer(identifier(), hostname(), port(), is_initiator);
    get_attestation().init();
}

void PeerHandler::on_network_message(yael::network::Socket::message_in_t &msg)
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
        m_enclave.handle_peer_message(identifier(), msg.data, msg.length);
        break;
    default:
        LOG(ERROR) << "Unknown encryption header field: " << static_cast<int>(encryption);
    }

    get_attestation().update();
}

void PeerHandler::set_attestation_context(sgx_ra_context_t context)
{
    m_enclave.set_attestation_context(identifier(), context);
}

void PeerHandler::send(const uint8_t *data, uint32_t length)
{
    bool result = false;
   
    try {
       result = socket().send(data, length);
    } catch(std::runtime_error &e) {
        LOG(ERROR) << e.what();
    }

    if(!result)
    {
        LOG(ERROR) << "Failed to send message to peer " << identifier();
    }
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
        bool ok = g_disk.read_undecrypted_from_disk(path, bstream);
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

}
}
