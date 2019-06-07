/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "RemoteParty.h"
#include "Enclave.h"
#include "Ledger.h"
#include "TransactionProxy.h"
#include "PendingBitstreamResponse.h"
#include "RemoteParties.h"
#include "RemotePartyRunner.h"
#include "logging.h"

#include "ias_ra.h"
#include "util/FunctionCallResult.h"
#include "util/EncryptionType.h"
#include "util/defines.h"
#include "util/ecp.h"
#include "util/keys.h"

#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#endif

#include <sgx_tkey_exchange.h>

namespace credb::trusted
{

RemoteParty::RemoteParty(Enclave &enclave, remote_party_id identifier)
: m_enclave(enclave), m_remote_parties(m_enclave.remote_parties()), m_ledger(m_enclave.ledger()),
  m_task_manager(m_enclave.task_manager()), m_local_identifier(identifier), m_identity(nullptr)
{
}

#if !defined(TEST) && !defined(FAKE_ENCLAVE)
RemoteParty::~RemoteParty()
{
    sgx_ra_close(m_attestation_context);
}
#else
RemoteParty::~RemoteParty() = default;
#endif

#ifdef FAKE_ENCLAVE
credb_status_t RemoteParty::decrypt(const uint8_t *in_data, uint32_t in_len, bitstream &inner)
{
    bitstream input;
    input.assign(in_data, in_len, true);
    input.move_to(0);

    EncryptionType encryption;
    uint32_t payload_size;
    input >> reinterpret_cast<etype_data_t &>(encryption);
    if(encryption == EncryptionType::Attestation)
    {
        // Communication is probably still unencrypted at this point
        inner.assign(in_data, in_len, true);
        return CREDB_SUCCESS;
    }
    input >> payload_size;
    inner.assign(input.current(), payload_size, true);
    return CREDB_SUCCESS;
}
#else
credb_status_t RemoteParty::decrypt(const uint8_t *in_data, uint32_t in_len, bitstream &inner)
{
    sgx_ec_key_128bit_t *sk_key = nullptr;
    bitstream input;
    input.assign(in_data, in_len, true);
    input.move_to(0);

    if(!get_encryption_key(&sk_key))
    {
        // Communication is probably still unencrypted at this point
        inner.assign(in_data, in_len, true);
        return CREDB_SUCCESS;
    }

    EncryptionType encryption;
    uint32_t payload_size;

    input >> reinterpret_cast<etype_data_t &>(encryption) >> payload_size;

    uint8_t *payload = nullptr;
    input.read_raw_data(&payload, payload_size);
    if(encryption != EncryptionType::Encrypted)
    {
        log_error("Failed to decrypt message: not encrypted");
        return CREDB_ERROR_UNEXPECTED;
    }

    std::array<uint8_t, SAMPLE_SP_IV_SIZE> aes_gcm_iv = { 0 };
    std::array<uint8_t, SGX_AESGCM_MAC_SIZE> tag;

    input >> tag;

    inner.resize(payload_size);

    auto ret = sgx_rijndael128GCM_decrypt(sk_key, payload, payload_size, reinterpret_cast<uint8_t *>(inner.data()), aes_gcm_iv.data(), SAMPLE_SP_IV_SIZE, nullptr, 0, reinterpret_cast<const sgx_aes_gcm_128bit_tag_t *>(tag.data()));

    if(ret != SGX_SUCCESS)
    {
        log_error("Failed to decrypt message: " + to_string(ret));
        return CREDB_ERROR_UNEXPECTED;
    }
    else
    {
        return CREDB_SUCCESS;
    }
}
#endif

void RemoteParty::handle_attestation_message_two(bitstream &input)
{
    sgx_ra_msg2_t *msg2 = nullptr;
    uint32_t msg2_size = 0;

    input >> msg2_size;
    input.read_raw_data(reinterpret_cast<uint8_t **>(&msg2), msg2_size);

    attestation_queue_msg2(local_identifier(), msg2, msg2_size);
}

void RemoteParty::handle_groupid_response(bitstream &input)
{
    bool result;
    input >> result;

    std::string name;
    input >> name;
   
    sgx_ec256_public_t public_key;
    input >> public_key;

    try
    {
        // Don't set identity in the second phase of attestation
        if(m_identity == nullptr)
        {
            m_identity = &m_enclave.identity_database().get(name, public_key);

            if(*m_identity == m_enclave.identity())
            {
                log_fatal("looks like we connected to ourselves");
            }

            m_remote_parties.set_identity(local_identifier(), *m_identity);
        }

        attestation_queue_groupid_result(local_identifier(), result);
    }
    catch(attestation_error &e)
    {
        log_warning(std::string("Attestation error: ") + e.what());
        disconnect();
    }
}

void RemoteParty::disconnect()
{
    disconnect_remote_party(local_identifier());
}

credb_status_t RemoteParty::encrypt(const bitstream &data, bitstream &encrypted)
{
    const uint32_t payload_len = data.size();

    sgx_ec_key_128bit_t *sk_key = nullptr;
    if(!get_encryption_key(&sk_key))
    {
        encrypted << static_cast<etype_data_t>(EncryptionType::PlainText);
        encrypted.write_raw_data(data.data(), data.size());
        return CREDB_SUCCESS;
    }

#ifdef FAKE_ENCLAVE
    encrypted << static_cast<etype_data_t>(EncryptionType::Encrypted);
    encrypted << payload_len;
    encrypted.write_raw_data(data.data(), data.size());

    return CREDB_SUCCESS;
#else
    
    sgx_aes_gcm_128bit_tag_t tag;
    encrypted.move_to(0);
    encrypted.resize(sizeof(etype_data_t) + sizeof(payload_len) + payload_len + sizeof(tag));

    std::array<uint8_t, SAMPLE_SP_IV_SIZE> aes_gcm_iv = {0};

    encrypted << static_cast<etype_data_t>(EncryptionType::Encrypted);
    encrypted << payload_len;

    // For some reason it wants a non const version of the data
    auto ret = sgx_rijndael128GCM_encrypt(sk_key, const_cast<uint8_t*>(reinterpret_cast<const uint8_t *>(data.data())), payload_len, encrypted.current(), aes_gcm_iv.data(), aes_gcm_iv.size(), nullptr, 0, &tag);

    if(ret != SGX_SUCCESS)
    {
        log_error("Failed to encrypt message: " + to_string(ret));
        return CREDB_ERROR_UNEXPECTED;
    }

    encrypted.move_to(sizeof(EncryptionType) + sizeof(payload_len) + payload_len);
    encrypted << tag;
#endif

    return CREDB_SUCCESS;
}

void RemoteParty::send_attestation_msg(const bitstream &msg)
{
    bitstream out;
    out << static_cast<etype_data_t>(EncryptionType::Attestation);
    out.write_raw_data(msg.data(), msg.size());

    send_to_remote_party(local_identifier(), out.data(), out.size());
}

void RemoteParty::send(const bitstream &msg)
{
    bitstream out;
    auto ret = encrypt(msg, out);

    if(ret == CREDB_SUCCESS)
    {
        send_to_remote_party(local_identifier(), out.data(), out.size());
    }
    else
    {
        log_error("Failed to encrypt message.");
    }
}

void RemoteParty::set_attestation_context(sgx_ra_context_t context)
{
    m_attestation_context = context;
}

void RemoteParty::lock()
{
    remote_party_lock(local_identifier());
}

void RemoteParty::unlock()
{
    remote_party_unlock(local_identifier());
}

void RemoteParty::notify_all()
{
    remote_party_notify_all(local_identifier());
}

void RemoteParty::wait()
{
    remote_party_wait(local_identifier()); // FIXME failure handling
}

void RemoteParty::handle_op_request(bitstream &input, bitstream &output, const OpContext &op_context)
{
    taskid_t task_id;
    operation_id_t op_id;
    OperationType op_type;

    input >> task_id >> op_id;
    input >> reinterpret_cast<op_data_t &>(op_type);

    bool is_async = (op_type == OperationType::CallProgram || op_type == OperationType::ExecuteCode) &&
                    !m_enclave.is_downstream_mode();

    if(!is_async)
    {
        output << static_cast<mtype_data_t>(MessageType::OperationResponse);
        output << task_id << op_id;
    }

    if(m_enclave.is_downstream_mode())
    {
        handle_request_downstream_mode(input, op_context, op_type, output);
    }
    else if(op_type == OperationType::CallProgram)
    {
        handle_call_request(input, op_context, task_id, op_id);
    }
    else if(op_type == OperationType::ExecuteCode)
    {
        handle_execute_request(input, op_context, task_id, op_id);
    }
    else
    {
        handle_request_upstream_mode(input, op_context, op_type, output);
    }
}

void RemoteParty::handle_request_downstream_mode(bitstream &input,
                                                 const OpContext &op_context,
                                                 OperationType op_type,
                                                 bitstream &output)
{
    // if (op_type == OperationType::Clear)
    //     m_ledger.clear(""); // FIXME: also clear the downstream
    switch(op_type)
    {
    case OperationType::GetObject:
    {
        handle_request_get_object(input, op_context, output, false);
        break;
    }
    case OperationType::HasObject:
    {
        handle_request_has_object(input, op_context, output);
        break;
    }
    case OperationType::GetObjectWithWitness:
    {
        handle_request_get_object(input, op_context, output, true);
        break;
    }
    case OperationType::SetTrigger:
    {
        std::string collection;
        input >> collection;

        auto res = m_enclave.set_trigger(collection, local_identifier());
        output << res;

        log_debug("Set downstream trigger");
        break;
    }
    case OperationType::UnsetTrigger:
    {
        std::string collection;
        input >> collection;

        auto res = m_enclave.unset_trigger(collection, local_identifier());
        output << res;
        break;
    }
    case OperationType::GetStatistics: // TODO not sure if we should return downstream stats?
    case OperationType::NOP:
    case OperationType::DiffVersions:
    case OperationType::AddToObject:
    case OperationType::PutObject:
    case OperationType::PutObjectWithoutKey:
    case OperationType::CreateIndex:
    case OperationType::CountObjects:
    case OperationType::DropIndex:
    case OperationType::RemoveObject:
    case OperationType::Clear:
    case OperationType::GetObjectHistory: // TODO handle downstream
    case OperationType::FindObjects: // TODO handle downstream
    case OperationType::ExecuteTransaction:
    case OperationType::OrderEvents: // TODO handle downstream
    {
        auto upstream = m_remote_parties.find<Peer>(m_enclave.get_upstream_id());

        if(upstream)
        {
            upstream->lock();

            taskid_t task_id = 0;
            auto op_id = upstream->get_next_operation_id();

            bitstream req;
            req << static_cast<mtype_data_t>(MessageType::ForwardedOperationRequest);
            req << task_id << op_id;
            input.move_to(0);
            req << input;

            upstream->send(req);
            credb::trusted::PendingBitstreamResponse resp(op_id, *upstream);
            resp.wait();

            upstream->unlock();
            output = resp.get_result();
        }
        else
        {
            log_error("No upstream!");
        }
        break;
    }
    default:
    {
        log_error("not supported OperationType: " + std::to_string(static_cast<uint8_t>(op_type)));
        output << false;
    }
    }
}

void RemoteParty::handle_execute_request(bitstream &input, const OpContext &op_context, taskid_t task_id, operation_id_t op_id)
{
    json::Document binary(input);

    uint32_t num_args = 0;
    input >> num_args;

    std::vector<std::string> args;
    args.resize(num_args);

    if(binary.get_type() != json::ObjectType::Binary)
    {
        log_error("Can't execute: not a binary");
        return;
    }

    for(uint32_t i = 0; i < num_args; ++i)
    {
        std::string arg;
        input >> arg;
        args[i] = arg;
    }

    // TODO vet function and set op_context accordingly
    (void)op_context;
    
    auto runner = std::make_shared<RemotePartyRunner>(m_enclave, shared_from_this(), task_id, op_id, binary.as_bitstream(), args, "", "");

    m_enclave.task_manager().register_task(runner);

    runner->run();

    // This will continue asynchornously in its own userspace thread from here
}

void RemoteParty::handle_call_request(bitstream &input, const OpContext &op_context, taskid_t task_id, operation_id_t op_id)
{
    std::string collection, key, path;
    input >> collection >> key >> path;

    std::vector<std::string> args;

    identity_uid_t transaction_root;
    transaction_id_t transaction_id;

    input >> args >> transaction_root >> transaction_id;

    auto data = m_ledger.prepare_call(op_context, collection, key, path);

    if(data.empty())
    {
        bitstream output;

        // FIXME task id?
        output << static_cast<mtype_data_t>(MessageType::OperationResponse);
        output << task_id << op_id;

        output << FunctionCallResult::PolicyRejected;
   
        send(output);
    }
    else
    {
        auto runner = std::make_shared<RemotePartyRunner>(m_enclave, shared_from_this(), task_id, op_id, std::move(data), args, collection, key, transaction_root, transaction_id);

        m_enclave.task_manager().register_task(runner);

        runner->run();

        // This will continue asynchornously in its own userspace thread from here...
    }
}

void RemoteParty::handle_request_upstream_mode(bitstream &input,
                                               const OpContext &op_context,
                                               OperationType op_type,
                                               bitstream &output)
{
    switch(op_type)
    {
    case OperationType::NOP:
    {
        // log_debug("Received NOP request, op_id=" + std::to_string(id));
        output << true;
        break;
    }
    case OperationType::GetStatistics:
    {
        json::Writer writer;
        auto &eio = m_enclave.encrypted_io();

        writer.start_map();
        writer.write_integer("num_files", eio.num_files());
        writer.write_integer("total_file_size", eio.total_file_size());
        writer.write_integer("num_collections", m_ledger.num_collections());
        writer.end_map();

        output << writer.make_document();
        break;

    }
    case OperationType::DumpEverything:
    {
        std::string filename;
        input >> filename;

        log_info("Start to dump everything to disk! Filename: " + filename);
        log_info("This should only be used for DEBUG purpose and when the server is idle!");
        bool ok = m_enclave.dump_everything(filename);
        log_info("Finished DumpEverything " + std::string(ok ? "OK" : "FAIL"));

        output << ok;
        break;
    }
    case OperationType::LoadEverything:
    {
        std::string filename;
        input >> filename;

        log_info("Start to load everything to disk! Filename: " + filename);
        log_info("This should only be used for DEBUG purpose and when the server is newly spawned "
                 "and idle!");
        bool ok = m_enclave.load_everything(filename);
        log_info("Finished LoadEverything " + std::string(ok ? "OK" : "FAIL"));

        output << ok;
        break;
    }
    case OperationType::CreateWitness:
    {
        std::vector<event_id_t> events;
        input >> events;

        Witness witness;
        bool res = m_ledger.create_witness(witness, events);

        if(res)
        {
            output << true << witness;

            log_debug("Generated witness");
        }
        else
        {
            output << false;
        }

        break;
    }
    case OperationType::DiffVersions:
    {
        std::string collection, key;
        version_number_t version1, version2;
        input >> collection >> key >> version1 >> version2;

        json::Diffs diffs;
        if(!m_ledger.diff(op_context, collection, key, diffs, version1, version2))
        {
            output << false;
            break;
        }

        output << true;
        output << static_cast<uint32_t>(diffs.size());

        for(auto &diff : diffs)
        {
            diff.compress(output);
        }

        break;
    }
    case OperationType::ListPeers:
    {
        auto infos = m_remote_parties.get_peer_infos();

        output << infos.size();

        for(auto &[name, hostname, port] : infos)
        {
            json::Writer writer;
            writer.start_map();
            writer.write_string("name", name);
            writer.write_string("hostname", hostname);
            writer.write_integer("port", port);
            writer.end_map();

            output << writer.make_document();
        }
        break;
    }
    case OperationType::SetTrigger:
    {
        std::string collection;
        input >> collection;

        auto res = m_ledger.set_trigger(collection, local_identifier());

        output << res;
        break;
    }
    case OperationType::UnsetTrigger:
    {
        std::string collection;
        input >> collection;

        auto res = m_ledger.unset_trigger(collection, local_identifier());

        output << res;
        break;
    }
    case OperationType::PutObjectWithoutKey:
    {
        std::string collection;
        input >> collection;

        uint32_t val_len;
        input >> val_len;

        uint8_t *value = nullptr;

        try {
            input.read_raw_data(&value, val_len);
        } catch(std::runtime_error &e) {
            log_error(std::string("received invalid message from client: ") + e.what());
            return;
        }

        json::Document doc(value, val_len, json::DocumentMode::Copy);

        std::string key;
        auto event_id = m_ledger.put_without_key(op_context, collection, key, doc);

        output << event_id << key;
        break;
    }
    case OperationType::AddToObject:
    case OperationType::PutObject:
    {
        std::string collection, full_path;
        input >> collection >> full_path;
        //         log_debug("Received put request, op_id=" + std::to_string(op_id) + " key=" +
        //         full_path);

        uint32_t val_len;
        input >> val_len;

        uint8_t *value = nullptr;

        try {
            input.read_raw_data(&value, val_len);
        } catch(std::runtime_error &e) {
            log_error(std::string("received invalid message from client: ") + e.what());
            return;
        }

        json::Document doc(value, val_len, json::DocumentMode::Copy);

        auto [key, path] = parse_path(full_path);

        if(!path.empty())
        {
            if(!m_ledger.has_object(collection, key))
            {
                log_debug("Cannot update object. Does not exist.");
                output << INVALID_EVENT;
                break;
            }
        }
        else
        {
            if(!m_ledger.is_valid_key(key))
            {
                // invalid key;
                log_debug("Cannot put object '" + key + "'. Invalid keyword.");
                output << INVALID_EVENT;
                break;
            }
        }

        event_id_t event_id;

        if(op_type == OperationType::AddToObject)
        {
            event_id = m_ledger.add(op_context, collection, key, doc, path);
        }
        else
        {
            event_id = m_ledger.put(op_context, collection, key, doc, path);
        }

        output << event_id;
        break;
    }
    case OperationType::RemoveObject:
    {
        std::string collection, key;
        input >> collection >> key;

        output << m_ledger.remove(op_context, collection, key);
        break;
    }
    case OperationType::GetObjectHistory:
    {
        std::string collection, key;
        input >> collection >> key;

        auto it = m_ledger.iterate(op_context, collection, key);

        output << true;
        uint32_t size = 0;
        auto size_pos = output.pos();
        output << size;

        ObjectEventHandle elem;

        while(it.next_handle(elem))
        {
            auto val = elem.value();
            output << val.data();
            size += 1;
        }

        auto end_pos = output.pos();
        output.move_to(size_pos);
        output << size;
        output.move_to(end_pos);

        break;
    }
    case OperationType::HasObject:
    {
        handle_request_has_object(input, op_context, output);
        break;
    }
    case OperationType::CheckObject:
    {
        handle_request_check_object(input, op_context, output);
        break;
    }
    case OperationType::GetObject:
    {
        // log_debug("Received get object request, op_id=" + std::to_string(id));
        handle_request_get_object(input, op_context, output, false);
        break;
    }
    case OperationType::GetObjectWithWitness:
    {
        handle_request_get_object(input, op_context, output, true);
        break;
    }
    case OperationType::CountObjects:
    {
        std::string collection;
        input >> collection;
        json::Document predicates(input);

        output << m_ledger.count_objects(op_context, collection, predicates);
        break;
    }
    case OperationType::CreateIndex:
    {
        std::string collection, name;
        std::vector<std::string> paths;
        input >> collection >> name >> paths;
        output << m_ledger.create_index(collection, name, paths);
        break;
    }
    case OperationType::DropIndex:
    {
        std::string collection, name;
        input >> collection >> name;
        output << m_ledger.drop_index(collection, name);
        break;
    }
    case OperationType::Clear:
    {
        std::string collection;
        input >> collection;
        output << m_ledger.clear(op_context, collection);
        break;
    }
    case OperationType::FindObjects:
    {
        std::string collection;
        json::Document predicates("");
        std::vector<std::string> projection;
        int32_t limit;

        input >> collection >> predicates >> projection >> limit;

        if(limit == 0)
        {
            log_fatal("Got invalid value for limit");
        }

        auto it = m_ledger.find(op_context, collection, predicates);

        uint32_t size = 0;
        uint32_t size_pos = output.pos();
        output << size;

        std::string key;
        ObjectEventHandle hdl;

        auto eid = it.next(key, hdl);

        for(; hdl.valid(); eid = it.next(key, hdl))
        {
            size += 1;
            output << key << eid;

            json::Document value = hdl.value();
            if(!projection.empty())
            {
                json::Document filtered(value, projection);
                output << filtered;
            }
            else
            {
                output << value;
            }

            if(limit > 0 && size == static_cast<uint32_t>(limit))
            {
                break;
            }
        }

        uint32_t end_pos = output.pos();
        output.move_to(size_pos);
        output << size;
        output.move_to(end_pos);
        break;
    }
    case OperationType::TransactionAbort:
    {
        identity_uid_t tx_root;
        transaction_id_t tx_id;

        input >> tx_root >> tx_id;

        auto &tx_mgr = m_enclave.transaction_manager();
        auto tx = tx_mgr.get(tx_root, tx_id);

        if(tx && !tx->is_done())
        {
            if(!tx->is_remote())
            {
                log_fatal("Cannot abort: not a remote transaction");
            }

            tx->abort();
            output << true;
        }
        else
        {
            output << false;
        }

        break;
    }
    case OperationType::TransactionPrepare:
    {
        identity_uid_t tx_root;
        transaction_id_t tx_id;
        bool generate_witness;

        input >> tx_root >> tx_id >> generate_witness;

        auto &tx_mgr = m_enclave.transaction_manager();
        auto tx = tx_mgr.get(tx_root, tx_id);

        if(tx)
        {
            if(!tx->is_remote())
            {
                log_fatal("Cannot prepare: not a remote transaction");
            }

            bool res = false;

            try {
                res = tx->prepare(generate_witness);
            }
            catch(std::runtime_error &e) {
                log_debug(e.what());
            }
            catch(would_block_exception&) {
                // not an error, but we need to abort
            }

            output << res;
        }
        else
        {
            output << false;
        }

        break;
    }
    case OperationType::TransactionCommit:
    {
        identity_uid_t tx_root;
        transaction_id_t tx_id;
        bool generate_witness;

        input >> tx_root >> tx_id >> generate_witness;

        auto &tx_mgr = m_enclave.transaction_manager();

        auto tx = tx_mgr.get(tx_root, tx_id);

        if(tx)
        {
            if(!tx->is_remote())
            {
                log_fatal("Cannot commit: not a remote transaction");
            }

            auto res = tx->commit(generate_witness);
            output << true << res;
        }
        else
        {
            log_warning("cannot commit: no such transaction");
            output << false;
        }

        break;
    }
    // This is only for clients
    case OperationType::ExecuteTransaction:
    {
        TransactionProxy tx(input, op_context, m_remote_parties, m_enclave.transaction_manager(), m_enclave);

        auto bs = tx.process();
        output << bs;
        break;
    }
    case OperationType::OrderEvents:
    {
        event_id_t first, second;
        input >> first >> second;

        auto res = order(first, second);

        if(res == OrderResult::Unknown)
        {
            // check if we can use transaction information to order the events
            auto &tx_ledger = m_enclave.transaction_ledger();

            auto ref1 = m_ledger.get_transaction_ref(first);
            auto ref2 = m_ledger.get_transaction_ref(second);

            if(ref1 != INVALID_LEDGER_POS && ref2 != INVALID_LEDGER_POS)
            {
                if(ref1 == ref2)
                {
                    // Part of the same transaction
                    res = OrderResult::Equal;
                }
                else
                {
                    auto tx1 = tx_ledger.get(ref1);
                    auto tx2 = tx_ledger.get(ref2);

                    res = order(tx1, tx2);
                }
            }
        }

        output << res;
        break;
    }
    case OperationType::TellPeerType:
    {
        // TODO: make this part of attestation
        PeerType peer_type;
        input >> peer_type;
        log_debug("OperationType::TellPeerType: local_identifier()=" + std::to_string(local_identifier()) +
                  " peer_type=" + std::to_string(peer_type));
        auto *self = dynamic_cast<Peer*>(this);
        self->set_peer_type(peer_type);
        bitstream bstream;

        log_debug("Sending disk key");
        const auto &disk_key = m_enclave.encrypted_io().disk_key();
        bstream.write_raw_data(reinterpret_cast<const uint8_t *>(disk_key), sizeof(disk_key));

        log_debug("Sending collection list");

        auto &cols = m_ledger.collections();
        uint32_t size = cols.size();
        bstream << size;

        for(auto &[name, col] : cols)
        {
            bstream << name;
            col.primary_index().serialize_root(bstream);
        }

        output << bstream;
        break;
    }
    default:
    {
        log_error("Received unknown operation from client");
        output << false;
        break;
    }
    }
}

void RemoteParty::handle_request_check_object(bitstream &input, const OpContext &op_context, bitstream &output)
{
    std::string collection, key;
    json::Document predicates;

    input >> collection >> key >> predicates;

    size_t ppos;
    std::string path;

    if((ppos = key.find(".")) != std::string::npos)
    {
        path = key.substr(ppos + 1, std::string::npos);
        key = key.substr(0, ppos);
    }

    bool res = m_ledger.check(op_context, collection, key, path, predicates);
    output << res;
}

void RemoteParty::handle_request_has_object(bitstream &input, const OpContext &op_context, bitstream &output)
{
    std::string collection, key;
    input >> collection >> key;

    std::string path;
    size_t ppos;

    if((ppos = key.find(".")) != std::string::npos)
    {
        path = key.substr(ppos + 1, std::string::npos);
        key = key.substr(0, ppos);
    }

    // FIXME no policy checking?
    (void)op_context;
    bool res = m_ledger.has_object(collection, key);
    output << res;
}

void RemoteParty::handle_request_get_object(bitstream &input, const OpContext &op_context, bitstream &output, bool generate_witness)
{
    std::string collection, full_path;
    input >> collection >> full_path;

    auto [key, path] = parse_path(full_path);

    auto it = m_ledger.iterate(op_context, collection, key, path);
    auto [eid, value] = it.next();

    bitstream bstream;

    bitstream &out = generate_witness ? bstream : output;

    if(eid == INVALID_EVENT)
    {
        out << INVALID_EVENT;
        // note this might be due to policy restrictions
        log_debug("Can't retrieve object " + collection + "/" + key);
    }
    else
    {
        out << eid << value;

        if(generate_witness)
        {
            Witness witness;
            bool ok = m_ledger.create_witness(witness, std::vector<event_id_t>{ eid });
            if(ok)
            {
                out << true << witness;
            }
            else
            {
                out << false;
            }
        }
    }

    if(generate_witness)
    {
        output << bstream;
    }
}

} // namespace credb::trusted
