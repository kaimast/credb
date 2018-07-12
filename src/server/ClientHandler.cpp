#include "ClientHandler.h"
#include "ClientAcceptor.h"
#include "PeerAcceptor.h"

#include "util/EncryptionType.h"
#include "util/OperationType.h"
#include "util/error.h"

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

namespace credb
{
namespace untrusted
{

ClientHandler::ClientHandler(ClientAcceptor &acceptor,
                             std::unique_ptr<yael::network::Socket> &&socket,
                             EnclaveHandle &enclave,
                             PeerAcceptor &peer_acceptor)
: RemoteParty(std::forward<std::unique_ptr<yael::network::Socket>>(socket), enclave, AttestationMode::OneWay),
  m_acceptor(acceptor), m_peer_acceptor(peer_acceptor)
{
    LOG(INFO) << "New client connected";
}

void ClientHandler::setup(remote_party_id identifier)
{
    set_identifier(identifier);
    m_enclave.init_client(identifier);
    get_attestation().init();
}

void ClientHandler::handle_plain_text_message(bitstream &msg)
{
    // FIXME add a signature to plaintext messages

    MessageType msg_type;
    OperationType op_type;
    taskid_t task_id;
    operation_id_t op_id;

    msg >> reinterpret_cast<mtype_data_t &>(msg_type);
    msg >> task_id >> op_id;
    msg >> reinterpret_cast<op_data_t &>(op_type);

    if(op_type != OperationType::Peer)
    {
        LOG(FATAL) << "Unknown op type";
    }

    // FIXME check if client is authorized to create new connections

    std::string remote_address;
    msg >> remote_address;

    m_peer_acceptor.connect(remote_address);

    bitstream response;
    response << static_cast<etype_data_t>(EncryptionType::PlainText);
    response << static_cast<mtype_data_t>(MessageType::OperationResponse);
    response << task_id << op_id << true;

    send(response);
}

void ClientHandler::on_network_message(yael::network::Socket::message_in_t &msg)
{
    bitstream peeker;
    peeker.assign(msg.data, msg.length, true);

    EncryptionType encryption;
    peeker >> reinterpret_cast<etype_data_t &>(encryption);

    switch(encryption)
    {
    case EncryptionType::PlainText:
        handle_plain_text_message(peeker);
        break;
    case EncryptionType::Encrypted:
    case EncryptionType::Attestation:
        // TODO keep track of client state.
        m_enclave.handle_message(identifier(), msg.data, msg.length);
        break;
    default:
        throw std::runtime_error("Unknown encryption type");
    }

    get_attestation().update();
    delete[] msg.data;
}

} // namespace untrusted
} // namespace credb
