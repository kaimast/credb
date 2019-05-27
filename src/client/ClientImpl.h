#pragma once

#include "credb/Client.h"

#include <condition_variable>
#include <experimental/optional>
#include <functional>
#include <sgx_key_exchange.h>
#include <unordered_map>
#include <yael/NetworkSocketListener.h>

#include "credb/ucrypto/ucrypto.h"
#include <bitstream.h>

#include "util/EncryptionType.h"
#include "util/MessageType.h"
#include "util/OperationType.h"
#include "util/keys.h"

namespace credb
{

enum class ClientState
{
    WaitingForGroupId,
    WaitingForMsg1,
    WaitingForMsg3,
    Connected,
    Failure,
    Closed
};

class ClientImpl : public Client, public yael::NetworkSocketListener
{
public:
    ClientImpl(const std::string &client_name, const std::string &server_name, const std::string &address, uint16_t port);

    ~ClientImpl() = default;

    // Set up TCP connection between Server and Client
    // Perform SGX attestation and setup encrypted channel
    void setup();

    void close() override;

    std::shared_ptr<Transaction> init_transaction(IsolationLevel isolation) override;

    bool create_witness(const std::vector<event_id_t> &events, Witness &witness);

    const sgx_ec256_public_t &get_server_cert() const override;
    std::string get_server_cert_base64() const override;

    cow::ValuePtr execute(const std::string &code, const std::vector<std::string> &args) override;

    bool peer(const std::string &remote_address) override;

    std::vector<json::Document> list_peers() override;

    bitstream receive_response(uint32_t msg_id);

    std::shared_ptr<Collection> get_collection(const std::string &name) override;

    // for debug purpose only
    bool nop(const std::string &garbage) override;
    bool dump_everything(const std::string &filename) override;
    bool load_everything(const std::string &filename) override;

    operation_id_t get_next_operation_id();
    bitstream generate_op_request(operation_id_t op_id, OperationType op_type);
    void send_encrypted(bitstream &msg);

    const std::string &name() const override { return m_name; }

    cow::MemoryManager &memory_manager() { return m_mem; }

    void set_trigger(const std::string &collection, std::function<void()> func);

    json::Document get_statistics() override;

    OrderResult order(const event_id_t &first, const event_id_t &second) override;

    // only called by clientimpl
    void unset_trigger(const std::string &collection);;

protected:
    void on_network_message(yael::network::Socket::message_in_t &msg) override;
    void on_disconnect() override;

private:
    friend class TransactionImpl;

    void send_plain(bitstream &msg);

    void decrypt(const uint8_t *data, uint32_t len, bitstream &out);
    void encrypt(bitstream &data, bitstream &outstream);

    void process_message_one(bitstream &input, bitstream &output);
    void process_message_three(bitstream &input, bitstream &output);

    void handle_message(bitstream &input, bitstream &output, std::unique_lock<std::mutex> &lock);

    void handle_attestation_message(bitstream &input, bitstream &output);
    void handle_operation_response(bitstream &input, bitstream &output);

    std::mutex m_mutex;

    /// Needed for function call responses
    cow::DummyMemoryManager m_mem;

    uint32_t m_db_groupid = 0;

    sgx_ec256_private_t m_dhke_private; // a
    sgx_ec256_public_t m_dhke_public; // g_a
    sgx_ec256_public_t m_dhke_server_public; // g_b

    sgx_ec_key_128bit_t m_vk_key; // Shared secret key for the REPORT_DATA
    sgx_ec_key_128bit_t m_mk_key; // Shared secret key for generating MAC's
    sgx_ec_key_128bit_t m_sk_key; // Shared secret key for encryption
    sgx_ec_key_128bit_t m_smk_key; // Used only for SIGMA protocol

    sgx_ps_sec_prop_desc_t m_ps_sec_prop;

    ClientState m_state;

    operation_id_t m_next_operation_id = 1;

    std::condition_variable m_socket_cond;

    std::unordered_map<operation_id_t, bitstream> m_responses;

    const std::string m_name;
    const std::string m_server_name;
    bool m_downstream_mode;

    std::string m_error_str;

    sgx_ec256_public_t m_client_public_key;
    sgx_ec256_public_t m_server_public_key;
    sgx_ec256_public_t m_upstream_public_key;

    sgx_ec256_private_t m_client_private_key;

    std::unordered_map<std::string, std::function<void()>> m_triggers;
};

inline operation_id_t ClientImpl::get_next_operation_id()
{
    auto ret = m_next_operation_id;
    m_next_operation_id++;
    return ret;
}

inline void ClientImpl::send_encrypted(bitstream &msg)
{
    bitstream encrypted;
    encrypt(msg, encrypted);

    this->send(encrypted.data(), encrypted.size());
}

inline bitstream ClientImpl::generate_op_request(operation_id_t op_id, OperationType op_type)
{
    taskid_t task_id = 0;

    bitstream req;
    req << static_cast<mtype_data_t>(MessageType::OperationRequest);
    req << task_id << op_id;
    req << static_cast<op_data_t>(op_type);

    return req;
}

inline void ClientImpl::send_plain(bitstream &msg)
{
    bitstream plain;
    plain << static_cast<etype_data_t>(EncryptionType::PlainText);
    plain.write_raw_data(msg.data(), msg.size());

    this->send(plain.data(), plain.size());
}

} // namespace credb
