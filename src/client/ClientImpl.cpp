/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#include <yael/EventLoop.h>

#include <cstddef>
#include <sgx_uae_service.h>
#include <stdbitstream.h>
#include <fstream>
#include <experimental/filesystem>
#include <iostream>

#include "credb/ucrypto/ucrypto.h"
#include "error.h"
#include "key_derivation.h"

#include "ClientImpl.h"
#include "CollectionImpl.h"

#include <cowlang/cow.h>
#include <yael/network/TcpSocket.h>

// FIXME
#include "../enclave/ias_ra.h"

#include "credb/base64.h"
#include "util/EncryptionType.h"
#include "util/defines.h"
#include "util/remote_attestation_result.h"

#include "TransactionImpl.h"

#include "DocParser.h"

#include "PendingDocumentResponse.h"
#include "PendingCallResponse.h"
#include "PendingResponse.h"
#include "PendingListResponse.h"
#include "PendingSetResponse.h"
#include "PendingSizeResponse.h"
#include "PendingWitnessResponse.h"

using sample_isv_svn_t = uint16_t;

using namespace yael;

using sample_enroll = int (*)(int sp_credentials, sample_spid_t *spid, int *authentication_token);

using sample_get_sigrl = int (*)(const sample_epid_group_id_t gid, uint32_t *p_sig_rl_size, uint8_t **p_sig_rl);

using sample_verify_attestation_evidence = int (*)(sample_quote_t *p_isv_quote,
                                                  uint8_t *pse_manifest,
                                                  ias_att_report_t *attestation_verification_report);

struct sample_extended_epid_group
{
    uint32_t extended_epid_group_id;
    sample_enroll enroll;
    sample_get_sigrl get_sigrl;
    sample_verify_attestation_evidence verify_attestation_evidence;
} ;

//.This.is.supported.extended.epid.group.of.SP..SP.can.support.more.than.one
//.extended.epid.group.with.different.extended.epid.group.id.and.credentials.
/*static const sgx_extended_epid_group g_extended_epid_groups[] = {
{
    0,
    ias_enroll,
    ias_get_sigrl,
    ias_verify_attestation_evidence
}
};*/

// FIXME: this should be set by the IAS server
sample_spid_t g_spid;
// static const sample_extended_epid_group* g_sp_extended_epid_group_id = nullptr;

namespace credb
{

ClientImpl::ClientImpl(std::string client_name, std::string server_name, const std::string &address, uint16_t port, bool unsafe_mode)
    : m_name(std::move(client_name)), m_server_name(std::move(server_name)), m_unsafe_mode(unsafe_mode)
{
    const std::string KEYS_FILENAME = m_name + ".identity";

    if(std::experimental::filesystem::exists(KEYS_FILENAME))
    {
        LOG(INFO) << "Loaded keys from file '" << KEYS_FILENAME << "'";

        std::ifstream data(KEYS_FILENAME, std::ifstream::in | std::ifstream::binary);

        data.read(reinterpret_cast<char*>(&m_client_private_key), sizeof(m_client_private_key));
        data.read(reinterpret_cast<char*>(&m_client_public_key),  sizeof(m_client_public_key));
    }
    else
    {
        sgx_ecc_state_handle_t ecc = nullptr;
        sgx_ecc256_open_context(&ecc);
        auto ret = sgx_ecc256_create_key_pair(&m_client_private_key, &m_client_public_key, ecc);
        sgx_ecc256_close_context(ecc);

        if(ret != SGX_SUCCESS)
        {
            LOG(FATAL) << "Failed to generate client key pair";
        }

        std::ofstream data(KEYS_FILENAME, std::ifstream::out | std::ifstream::binary);
        data.write(reinterpret_cast<const char*>(&m_client_private_key), sizeof(m_client_private_key));
        data.write(reinterpret_cast<const char*>(&m_client_public_key), sizeof(m_client_public_key));
    }

    if(port == 0)
    {
        port = CLIENT_PORT;
    }

    auto sock = std::make_unique<yael::network::TcpSocket>();
    auto net_addr = yael::network::resolve_URL(address, port);

    bool ok = sock->connect(net_addr);

    if(ok)
    {
        set_socket(std::move(sock), yael::SocketType::Connection);
        m_state = ClientState::WaitingForGroupId;
    }
    else
    {
        throw std::runtime_error("TCP connection failed");
    }
}

void ClientImpl::close()
{
    std::unique_lock lock(m_mutex);

    if(socket().is_valid())
    {
        m_state = ClientState::Closed;
        NetworkSocketListener::close_socket();
    }
}

void ClientImpl::setup()
{
    std::unique_lock lock(m_mutex);

    while(m_state != ClientState::Connected && m_state != ClientState::Failure
            && socket().is_connected())
    {
        m_socket_cond.wait(lock);
    }

    if(!socket().is_connected() || m_state == ClientState::Failure)
    {
        throw std::runtime_error("Failed to setup server connection: " + m_error_str);
    }
}

std::vector<json::Document> ClientImpl::list_peers()
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::ListPeers);

    send_encrypted(req);

    PendingListResponse<json::Document> resp(op_id, *this);
    resp.wait();

    return std::move(resp.result());
}

bool ClientImpl::peer(const std::string &remote_address)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::Peer);
    req << remote_address;

    send_plain(req);

    PendingBooleanResponse resp(op_id, *this);
    resp.wait();

    return resp.result();
}

cow::ValuePtr ClientImpl::execute(const std::string &code, const std::vector<std::string> &args)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::ExecuteCode);

    bitstream bstream = cow::compile_string(code);
    json::Binary binary(bstream);

    binary.compress(req);

    req << static_cast<uint32_t>(args.size());
    for(auto &arg : args)
    {
        req << arg;
    }

    send_encrypted(req);

    PendingCallResponse resp(op_id, *this, memory_manager());
    resp.wait();

    if(resp.success())
    {
        return resp.return_value();
    }
    else
    {
        throw std::runtime_error("ClientImpl Execute failed: [" + resp.error() + "]");
    }
}

const sgx_ec256_public_t &ClientImpl::get_server_cert() const
{
    return m_downstream_mode ? m_upstream_public_key : m_server_public_key;
}

std::string ClientImpl::get_server_cert_base64() const
{
    return base64_encode(reinterpret_cast<unsigned char const *>(&get_server_cert()),
                         sizeof(get_server_cert()));
}

std::shared_ptr<Transaction> ClientImpl::init_transaction(IsolationLevel isolation)
{
    return std::make_shared<TransactionImpl>(*this, isolation);
}

void ClientImpl::encrypt(bitstream &data, bitstream &outstream)
{
    if(m_unsafe_mode)
    {
        uint32_t len = data.size();

        outstream.move_to(0);
        outstream.resize(sizeof(etype_data_t) + sizeof(len) + len);
        outstream << static_cast<etype_data_t>(EncryptionType::Encrypted);
        outstream << len;
        outstream.write_raw_data(data.data(), data.size());
    }
    else
    {
        uint32_t len = data.size();

        std::array<uint8_t, SAMPLE_SP_IV_SIZE> aes_gcm_iv = { 0 };
        std::array<uint8_t, SGX_AESGCM_MAC_SIZE> tag;

        outstream.move_to(0);
        outstream.resize(sizeof(etype_data_t) + sizeof(len) + len + sizeof(tag));
        outstream << static_cast<etype_data_t>(EncryptionType::Encrypted);
        outstream << len;

        auto ret = sgx_rijndael128GCM_encrypt(&m_sk_key, const_cast<const uint8_t*>(data.data()), len, outstream.current(), aes_gcm_iv.data(), SAMPLE_SP_IV_SIZE, nullptr, 0, reinterpret_cast<sgx_aes_gcm_128bit_tag_t*>(tag.data()));

        outstream.move_to(sizeof(etype_data_t) + sizeof(len) + len);
        outstream << tag;

        if(ret != SGX_SUCCESS)
        {
            LOG(FATAL) << "Encryption failed!";
        }
    }
}

bool ClientImpl::create_witness(const std::vector<event_id_t> &events, Witness &witness)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::CreateWitness);
    req << events;

    send_encrypted(req);

    PendingWitnessResponse resp(op_id, *this, witness);
    resp.wait();

    return resp.success();
}

bool ClientImpl::nop(const std::string &garbage)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::NOP);
    req << garbage;

    send_encrypted(req);
    PendingBooleanResponse resp(op_id, *this);
    resp.wait();

    return resp.result();
}

bool ClientImpl::dump_everything(const std::string &filename)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::DumpEverything);
    req << filename;

    send_encrypted(req);
    PendingBooleanResponse resp(op_id, *this);
    resp.wait();
    return resp.result();
}

void ClientImpl::set_trigger(const std::string &collection, std::function<void()> func)
{
    std::unique_lock lock(m_mutex);
    m_triggers.emplace(collection, func);
}

json::Document ClientImpl::get_statistics()
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::GetStatistics);

    send_encrypted(req);

    PendingDocumentResponse resp(op_id, *this);
    resp.wait();

    return resp.document();
}

void ClientImpl::unset_trigger(const std::string &collection)
{
    std::unique_lock lock(m_mutex);
    auto it = m_triggers.find(collection);

    if(it == m_triggers.end())
    {
        throw std::runtime_error("no such trigger");
    }

    m_triggers.erase(it);
}

bool ClientImpl::load_everything(const std::string &filename)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::LoadEverything);
    req << filename;

    send_encrypted(req);
    PendingBooleanResponse resp(op_id, *this);
    resp.wait();
    return resp.result();
}

bitstream ClientImpl::receive_response(uint32_t msg_id)
{
    std::unique_lock lock(m_mutex);
    auto it = m_responses.find(msg_id);

    while(it == m_responses.end())
    {
        m_socket_cond.wait(lock);
        it = m_responses.find(msg_id);
    }

    auto resp = std::move(it->second);
    m_responses.erase(it);

    return resp;
}

void ClientImpl::handle_attestation_message(bitstream input, bitstream &output)
{
    MessageType type;
    input >> reinterpret_cast<mtype_data_t &>(type);

    switch(type)
    {
    case MessageType::TellGroupId:
    {
        if(m_state != ClientState::WaitingForGroupId)
        {
            throw std::runtime_error("Client and server state mismatch!");
        }

        std::string name;

        input >> m_db_groupid;
        input >> name;
        input >> m_server_public_key;
        input >> m_downstream_mode;
        
        if(m_downstream_mode)
        {
            input >> m_upstream_public_key;
        }

        if(name != m_server_name)
        {
            // FIXME also check certificates
            this->close_socket();
            m_state = ClientState::Failure;
            m_error_str = "Server names don't match. Cowardly refusing to connect.";
        }
        else
        {
            // TODO contact Intel Attestation Service
            output << EncryptionType::Attestation;
            output << static_cast<mtype_data_t>(MessageType::GroupIdResponse);
            output << true;

            output << m_name;
            output << m_client_public_key;

            m_state = ClientState::WaitingForMsg1;
        }
        break;
    }
    case MessageType::AttestationMessage1:
    {
        if(m_state != ClientState::WaitingForMsg1)
        {
            throw std::runtime_error("Client and server state mismatch!");
        }

        process_message_one(input, output);
        break;
    }
    case MessageType::AttestationMessage3:
    {
        if(m_state != ClientState::WaitingForMsg3)
        {
            throw std::runtime_error("Client and server state mismatch!");
        }

        process_message_three(input, output);
        break;
    }
    default:
    {
        LOG(ERROR) << "Received unknown message type";
        return;
    }
    };
}

void ClientImpl::handle_message(bitstream input, bitstream &output,
        std::unique_lock<std::mutex> &lock)
{
    if(m_state != ClientState::Connected)
    {
        LOG(FATAL) << "Not connected yet";
    }

    MessageType type;
    input >> reinterpret_cast<mtype_data_t &>(type);

    switch(type)
    {
    case MessageType::OperationResponse:
        handle_operation_response(std::move(input), output);
        break;
    case MessageType::NotifyTrigger:
    {
        std::string collection;
        input >> collection;

        auto it = m_triggers.find(collection);

        if(it == m_triggers.end())
        {
            break;
        }

        auto func = it->second;
        
        lock.unlock();
        func();
        lock.lock();
        break;
    }
    default:
        throw std::runtime_error("Failed to handle message in connected state");
    }
}

void ClientImpl::handle_operation_response(bitstream input, bitstream &output)
{
    taskid_t task_id;
    operation_id_t op_id;
    input >> task_id >> op_id;

    m_responses.emplace(op_id, std::move(input));
    (void)output;
}

void ClientImpl::on_network_message(yael::network::message_in_t &msg)
{
    std::unique_lock lock(m_mutex);

    bitstream input, output;

    if(msg.length < (sizeof(MessageType) + sizeof(EncryptionType)))
    {
        LOG(ERROR) << "Received too small message!";
        return;
    }

    bitstream peeker;
    peeker.assign(msg.data, msg.length, true);
    EncryptionType encryption;
    peeker >> reinterpret_cast<etype_data_t &>(encryption);

    switch(encryption)
    {
    case EncryptionType::Attestation:
        // consume the encryption field
        input.assign(msg.data, msg.length, true);
        input >> reinterpret_cast<etype_data_t &>(encryption);
        handle_attestation_message(std::move(input), output);
        break;
    case EncryptionType::Encrypted:
        decrypt(msg.data, msg.length, input);
        handle_message(std::move(input), output, lock);
        break;
    case EncryptionType::PlainText:
        handle_message(std::move(peeker), output, lock);
        break; // no encryption in fake enclave mode
    default:
        LOG(ERROR) << "Unknown encryption header field: " << static_cast<int>(encryption);
    }

    if(!output.empty())
    {
        this->send(output.data(), output.size());
    }

    m_socket_cond.notify_all();
}

void ClientImpl::on_disconnect()
{
    std::unique_lock lock(m_mutex);

    if(m_state != ClientState::Failure
       && m_state != ClientState::Closed)
    {
        LOG(FATAL) << "Lost connection to server";
    }
}

void ClientImpl::decrypt(const uint8_t *data, uint32_t len, bitstream &out)
{
    if(m_state != ClientState::Connected)
    {
        // No encryption key negotiated yet.
        out.assign(const_cast<uint8_t *>(data), len, false);
        return;
    }

    bitstream input;

    input.assign(const_cast<uint8_t *>(data), len, false);

    EncryptionType encryption;
    input >> reinterpret_cast<etype_data_t &>(encryption);

    uint32_t payload_len = 0;
    input >> payload_len;

    if(m_unsafe_mode)
    {
        out = bitstream(const_cast<uint8_t *>(input.current()), payload_len);
        return;
    }
    
    if(encryption != EncryptionType::Encrypted)
    {
        LOG(FATAL) << "Failed to decrypt message: not encrypted";
        return;
    }

    uint8_t *payload = nullptr;
    input.read_raw_data(&payload, payload_len);

    std::array<uint8_t, SAMPLE_SP_IV_SIZE> aes_gcm_iv = { 0 };
    std::array<uint8_t, SGX_AESGCM_MAC_SIZE> tag;

    input >> tag;

    out.resize(payload_len);

    auto ret = sgx_rijndael128GCM_decrypt(&m_sk_key, payload, payload_len, out.data(), aes_gcm_iv.data(), SAMPLE_SP_IV_SIZE, nullptr, 0, reinterpret_cast<const sgx_aes_gcm_128bit_tag_t *>(tag.data()));

    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "Could not decode server message: " << to_string(ret);
    }
}

void ClientImpl::process_message_one(bitstream &input, bitstream &output)
{
    sgx_ra_msg1_t msg1;
    sgx_ra_msg2_t msg2;

    input >> msg1;

    /* TODO IAS
    // Check to see if we have registered?
    if (!g_is_sp_andle_message(turn SP_UNSUPPORTED_EXTENDED_EPID_GROUP;
    }*/

    sgx_ecc_state_handle_t ecc_state = nullptr;

    // Get the sig_rl from attestation server using GID.
    // GID is Base-16 encoded of EPID GID in little-endian format.
    // In the product, the SP and attesation server uses an established channel for
    // communication./

    // TODO implement this!
    // msg2.sig_rl = nullptr;
    msg2.sig_rl_size = 0;

    // Need to save the client's public ECCDH key to local storage
    if(memcpy_s(&m_dhke_server_public, sizeof(m_dhke_server_public), &msg1.g_a, sizeof(msg1.g_a)))
    {
        LOG(FATAL) << " cannot do memcpy";
    }

    // Generate the Service providers ECCDH key pair.
    auto ret = sgx_ecc256_open_context(&ecc_state);
    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << " cannot get ECC context";
    }

    ret = sgx_ecc256_create_key_pair(&m_dhke_private, &m_dhke_public, ecc_state);
    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "cannot generate key pair";
    }

    if(!m_unsafe_mode)
    {
        // Generate the client/SP shared secret
        sgx_ec256_dh_shared_t dh_key;
        ret = sgx_ecc256_compute_shared_dhkey(&m_dhke_private, const_cast<sgx_ec256_public_t *>(&m_dhke_server_public), reinterpret_cast<sgx_ec256_dh_shared_t*>(&dh_key), ecc_state);

        if(ret != SGX_SUCCESS)
        {
            LOG(FATAL) << "Failed to compute shared key";
        }

        // smk is only needed for msg2->generation.
        auto derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_SMK, &m_smk_key);
        if(!derive_ret)
        {
            LOG(FATAL) << "Failed to derive SMK key";
        }

        // The rest of the keys are the shared secrets for future communication.
        derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_MK, &m_mk_key);
        if(!derive_ret)
        {
            LOG(FATAL) << "Failed to derive MK key";
        }

        derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_SK, &m_sk_key);
        if(!derive_ret)
        {
            LOG(FATAL) << "Failed to derive SK key";
        }

        derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_VK, &m_vk_key);
        if(!derive_ret)
        {
            LOG(FATAL) << "Failed to derive VK key";
        }

        memcpy_s(&msg2.g_b, sizeof(msg2.g_b), &m_dhke_public, sizeof(m_dhke_public));
        memcpy_s(&msg2.spid, sizeof(sample_spid_t), &g_spid, sizeof(g_spid));

        // The service provider is responsible for selecting the proper EPID
        // signature type and to understand the implications of the choice!
        msg2.quote_type = SAMPLE_QUOTE_LINKABLE_SIGNATURE;

#ifdef SUPPLIED_KEY_DERIVATION
    // isv defined key derivation function id
#define ISV_KDF_ID 2
        msg2.kdf_id = ISV_KDF_ID;
#else
        msg2.kdf_id = SAMPLE_AES_CMAC_KDF_ID;
#endif
    }

    std::array<sgx_ec256_public_t, 2> gb_ga;

    if(memcpy_s(&gb_ga[0], sizeof(gb_ga[0]), &m_dhke_public, sizeof(m_dhke_server_public)) ||
       memcpy_s(&gb_ga[1], sizeof(gb_ga[1]), &m_dhke_server_public, sizeof(m_dhke_public)))
    {
        LOG(FATAL) << "memcpy failed";
    }

    // Sign gb_ga
    ret = sgx_ecdsa_sign(reinterpret_cast<uint8_t*>(&gb_ga), sizeof(gb_ga), reinterpret_cast<sgx_ec256_private_t*>(&m_client_private_key), &msg2.sign_gb_ga, ecc_state);

    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "failed to sign ga_gb";
    }

    // Generate the CMACsmk for gb||SPID||TYPE||KDF_ID||Sigsp(gb,ga)
    std::array<uint8_t, SAMPLE_EC_MAC_SIZE> mac = { 0 };
    uint32_t cmac_size = offsetof(sgx_ra_msg2_t, mac);

    ret = sgx_rijndael128_cmac_msg(&m_smk_key, reinterpret_cast<const uint8_t *>(&msg2), cmac_size, reinterpret_cast<sgx_cmac_128bit_tag_t*>(mac.data()));

    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "cmac fail in";
    }

    memcpy(reinterpret_cast<void*>(msg2.mac), reinterpret_cast<void*>(mac.data()), mac.size());

    // FIXME make sure context is always closed
    if(ecc_state)
    {
        sgx_ecc256_close_context(ecc_state);
    }

    // FIXME: Implement error handling
    output << EncryptionType::Attestation;
    output << MessageType::AttestationMessage2;
    output << static_cast<uint32_t>(sizeof(msg2) + msg2.sig_rl_size);
    output.write_raw_data(reinterpret_cast<const uint8_t *>(&msg2), sizeof(msg2) + msg2.sig_rl_size);
    m_state = ClientState::WaitingForMsg3;

    // FIXME: Implement error handling
    // return status;
}

OrderResult ClientImpl::order(const event_id_t &first, const event_id_t &second)
{
    auto op_id = get_next_operation_id();
    auto req = generate_op_request(op_id, OperationType::OrderEvents);

    req << first << second;

    send_encrypted(req);
  
    PendingOrderResponse resp(op_id, *this);

    resp.wait();

    return resp.result(); 
}

void ClientImpl::process_message_three(bitstream &input, bitstream &output)
{
    sgx_ra_msg3_t *msg3 = nullptr;
    uint32_t msg3_size;

    input >> msg3_size;

    if(m_unsafe_mode)
    {
        (void)msg3;
        m_state = ClientState::Connected;

        output << EncryptionType::Attestation;
        output << MessageType::AttestationResult;
        output << 0;
        output << 0;

        return;
    }

    sample_report_data_t report_data = { 0 };

    input.read_raw_data(reinterpret_cast<uint8_t **>(&msg3), msg3_size);

    // Compare g_a in message 3 with local g_a.
    if(memcmp(&m_dhke_server_public, &msg3->g_a, sizeof(sgx_ec256_public_t)) != 0)
    {
        LOG(ERROR) << "Error, g_a is not same";
    }

    // Make sure that msg3_size is bigger than sample_mac_t.
    // (Mac is at the beginning of th emessage)
    uint32_t mac_size = msg3_size - sizeof(sample_mac_t);
    auto msg_cmaced = reinterpret_cast<const uint8_t *>(msg3);
    msg_cmaced += sizeof(sample_mac_t);

    // Verify the message mac using SMK
    sgx_cmac_128bit_tag_t mac = { 0 };
    auto ret = sgx_rijndael128_cmac_msg(&m_smk_key, msg_cmaced, mac_size, &mac);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Error, cmac fail";
    }

    // In real implementation, should use a time safe version of memcmp here,
    // in order to avoid side channel attack.
    if(memcmp(&msg3->mac[0], &mac[0], sizeof(mac)) != 0)
    {
        LOG(ERROR) << "Error, verify cmac fail";
    }

    if(memcpy_s(&m_ps_sec_prop, sizeof(m_ps_sec_prop), &msg3->ps_sec_prop, sizeof(msg3->ps_sec_prop)) != 0)
    {
        LOG(ERROR) << "Error, memcpy failed";
    }

    auto p_quote = reinterpret_cast<const sample_quote_t *>(msg3->quote);

    // Check the quote version if needed. Only check the Quote.version field if the enclave
    // identity fields have changed or the size of the quote has changed.  The version may
    // change without affecting the legacy fields or size of the quote structure.
    // if(p_quote->version < ACCEPTED_QUOTE_VERSION)
    //{
    //    fprintf(stderr,"\nError, quote version is too old.", __FUNCTION__);
    //    ret = SP_QUOTE_VERSION_ERROR;
    //    break;
    //}

    // Verify the report_data in the Quote matches the expected value.
    // The first 32 bytes of report_data are SHA256 HASH of {ga|gb|vk}.
    // The second 32 bytes of report_data are set to zero.
    sgx_sha_state_handle_t sha_handle = nullptr;

    ret = sgx_sha256_init(&sha_handle);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "init hash failed";
    }
    ret = sgx_sha256_update(reinterpret_cast<uint8_t *>(&m_dhke_server_public), sizeof(m_dhke_public), sha_handle);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Update hash failed";
    }

    ret = sgx_sha256_update(reinterpret_cast<uint8_t *>(&m_dhke_public), sizeof(m_dhke_server_public), sha_handle);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Update hash failed";
    }

    ret = sgx_sha256_update(reinterpret_cast<uint8_t *>(&m_vk_key), sizeof(m_vk_key), sha_handle);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Update hash failed";
    }
    ret = sgx_sha256_get_hash(sha_handle, (sgx_sha256_hash_t *)&report_data);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Generate hash failed";
    }


    if(memcmp((uint8_t *)&report_data, (uint8_t *)&(p_quote->report_body.report_data), sizeof(report_data)) != 0)
    {
        LOG(ERROR) << "Compare hash failed";
    }

    // Verify Enclave policy (an attestation server may provide an API for this if we
    // registered an Enclave policy)

    // Verify quote with attestation server.
    // In the product, an attestation server could use a REST message and JSON formatting to request
    // attestation Quote verification.  The sample only simulates this interface.
    // FIXME IAS Stuff
    //    ret = g_sp_extended_epid_group_id->verify_attestation_evidence(p_quote, nullptr,
    //    &attestation_report);
    if(ret != SGX_SUCCESS)
    {
        throw std::runtime_error("ias verification failed");
    }

    ias_att_report_t attestation_report;
    // attestation_report.info_blob;
    // LOG(INFO) << "pse_status: " << attestation_report.pse_status;

    // Note: This sample always assumes the PIB is sent by attestation server.  In the product
    // implementation, the attestation server could only send the PIB for certain attestation
    // report statuses.  A product SP implementation needs to handle cases
    // where the PIB is zero length.

    // In a product implementation of attestation server, the HTTP response header itself could have
    // an RK based signature that the service provider needs to check here.

    // The platform_info_blob signature will be verified by the client
    // when sent. No need to have the Service Provider to check it.  The SP
    // should pass it down to the application for further analysis.

    // A product service provider needs to verify that its enclave properties
    // match what is expected.  The SP needs to check these values before
    // trusting the enclave.  For the sample, we always pass the policy check.
    // Attestation server only verifies the quote structure and signature.  It does not
    // check the identity of the enclave.
    bool isv_policy_passed = true;

    // Assemble Attestation Result Message
    // Note, this is a structure copy.  We don't copy the policy reports
    // right now.
    auto &info_blob = attestation_report.info_blob;
    sample_mac_t blob_mac;

    // Generate mac based on the mk key.
    ret = sgx_rijndael128_cmac_msg(&m_mk_key, reinterpret_cast<const uint8_t *>(&info_blob),
                                   sizeof(info_blob), &blob_mac);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "cmac fail in [%s].";
    }

    //  if((IAS_QUOTE_OK == attestation_report.status) &&
    //        (IAS_PSE_OK == attestation_report.pse_status) &&
    //      (isv_policy_passed == true))
    if(isv_policy_passed)
    {
        output << EncryptionType::Attestation;
        output << MessageType::AttestationResult;
        output << 0;
        output << 0;

        output << info_blob;
        output << blob_mac;

        m_state = ClientState::Connected;
    }
    else
    {
        throw std::runtime_error("Attestation failed!");
    }

    sgx_sha256_close(sha_handle);
}

std::shared_ptr<Collection> ClientImpl::get_collection(const std::string &name)
{
    return std::make_shared<CollectionImpl>(*this, name);
}

std::shared_ptr<Client>
create_client(const std::string &client_name, const std::string &server_name, const std::string &server_addr, uint16_t server_port, bool unsafe_mode)
{
    if(client_name.empty())
    {
        throw std::runtime_error("Client name cannot be empty");
    }

    EventLoop::initialize(); // FIXME we need multiple background threads thanks to triggers
    auto &el = EventLoop::get_instance();

    try {
        auto c = el.make_event_listener<ClientImpl>(client_name, server_name, server_addr, server_port, unsafe_mode);

        if(!c->is_valid())
        {
            LOG(FATAL) << "Failed to create client";
        }

        c->setup();
        return c;
    }
    catch(std::exception &e)
    {
        throw std::runtime_error(std::string("Failed to connect to server ") + server_addr + ":" + std::to_string(server_port) + " " + e.what());
    }
}

} // namespace credb
