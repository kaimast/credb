#include "Enclave.h"
#include "PendingBitstreamResponse.h"
#include "RemoteEncryptedIO.h"

#include <sgx_utils.h>

#include "Ledger.h"
#include "Peers.h"
#include "PendingBooleanResponse.h"
#include "logging.h"
#include "util/EncryptionType.h"
#include "util/remote_attestation_result.h"

#ifdef FAKE_ENCLAVE
#include "../src/server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#endif

namespace credb
{
namespace trusted
{

Enclave *g_enclave;

static constexpr size_t BUFFER_SIZE = 70 << 20;

Enclave::Enclave()
: m_encrypted_io(new EncryptedIO), m_buffer_manager(m_encrypted_io.get(), "buffer", BUFFER_SIZE), m_ledger(*this), m_identity(nullptr)
#ifndef TEST
 , m_peers(*this), m_clients(*this)
#endif
{
}

credb_status_t Enclave::init(const std::string &name)
{
    if(m_identity != nullptr)
    {
        log_error("cannot initialize enclave twice!");
        return CREDB_ERROR_UNEXPECTED;
    }

    m_identity = &m_identity_database.get(name);

    // TODO load previous state (if any) from disk

    // generate node id.
    // sgx_read_rand(m_identifier.value, m_identifier.SIZE);

    sgx_ecc_state_handle_t ecc_state = nullptr;
    auto ret = sgx_ecc256_open_context(&ecc_state);

#ifdef FAKE_ENCLAVE
    if(ret != Status::SUCCESS)
#else
    if(ret != SGX_SUCCESS)
#endif
    {
        return CREDB_ERROR_UNEXPECTED;
    }

    ret = sgx_ecc256_create_key_pair(&m_private_key, &m_public_key, ecc_state);


#ifdef FAKE_ENCLAVE
    if(ret != Status::SUCCESS)
#else
    if(ret != SGX_SUCCESS)
#endif
    {
        return CREDB_ERROR_UNEXPECTED;
    }

#ifndef TEST
#ifndef FAKE_ENCLAVE
    sgx_report_t report;
    sgx_create_report(NULL, NULL, &report);

    sgx_key_request_t key_request;
    key_request.key_name = SGX_KEYSELECT_SEAL;
    key_request.key_policy = SGX_KEYPOLICY_MRENCLAVE;
    key_request.isv_svn = report.body.isv_svn;
    key_request.cpu_svn = report.body.cpu_svn;
    key_request.reserved1 = 0;
    key_request.attribute_mask = { 0, 0 };
    key_request.misc_mask = 0;
    //    key_request.reserved2 = 0;

    memset(&key_request.reserved2, 0, sizeof(key_request.reserved2));
    memset(&key_request.key_id, 0, sizeof(sgx_key_id_t));

    sgx_aes_gcm_128bit_key_t disk_key;
    auto res = sgx_get_key(&key_request, &disk_key);
    m_encrypted_io->set_disk_key(disk_key);

    if(res == SGX_SUCCESS)
    {
        log_info("generated disk key");
    }
    else
    {
        log_error("Failed to derive disk key: " + to_string(res));
        return CREDB_ERROR_KEYGEN_FAILED;
    }
#endif
#endif

    sgx_ecc256_close_context(ecc_state);
    m_buffer_manager.flush_all_pages();

    return CREDB_SUCCESS;
}

void Enclave::set_upstream(remote_party_id upstream_id)
{
    if(m_downstream_mode)
    {
        log_error("Downstream mode has already been set before");
        abort();
    }
    m_downstream_mode = true;
    m_upstream_id = upstream_id;

#ifndef TEST
    auto upstream = m_peers.find(m_upstream_id);
    upstream->lock();
    m_upstream_public_key = upstream->public_key();

    taskid_t task_id = 0;
    auto op_id = upstream->get_next_operation_id();
    bitstream req;
    req << static_cast<mtype_data_t>(MessageType::OperationRequest);
    req << task_id << op_id;
    req << static_cast<op_data_t>(OperationType::TellPeerType);
    req << PeerType::DownstreamServer;

    log_debug("send to upstream");
    upstream->send(req);
    credb::trusted::PendingBitstreamResponse resp(op_id, *upstream);
    resp.wait();
    upstream->unlock();
    log_debug("recv from upstream");

    auto bstream = resp.get_result();
    uint8_t *disk_key = nullptr;
    bstream.read_raw_data(&disk_key, sizeof(sgx_aes_gcm_128bit_key_t));

    auto *remote_io = new RemoteEncryptedIO(*this, *reinterpret_cast<sgx_aes_gcm_128bit_key_t *>(disk_key));
    m_buffer_manager.set_encrypted_io(remote_io);
    m_encrypted_io.reset(remote_io);
    log_info("Disk key loaded from upstream");

    m_ledger.clear_cached_blocks();

    std::vector<std::string> collection_names;
    bstream >> collection_names;
    m_ledger.load_upstream_index_root(collection_names);
#endif

    log_info("successfully connected to the upstream");
}

bool Enclave::is_downstream_mode() const { return m_downstream_mode; }


bool Enclave::read_from_disk(const std::string &filename, bitstream &data)
{
    if(m_downstream_mode)
    {
        return read_from_upstream_disk(filename, data);
    }
    else
    {
        return read_from_local_disk(filename, data);
    }
}

bool Enclave::read_from_upstream_disk(const std::string &filename, bitstream &data)
{
#ifdef TEST
    (void)filename;
    (void)data;
    return false;
#else
    auto upstream = m_peers.find(m_upstream_id);
    upstream->lock();
    auto op_id = upstream->get_next_operation_id();
    bitstream req;
    req << EncryptionType::PlainText;
    req << static_cast<mtype_data_t>(MessageType::OperationRequest);
    req << op_id;
    req << static_cast<op_data_t>(OperationType::ReadFromUpstreamDisk);
    req << filename;

    send_to_remote_party(m_upstream_id, req.data(), req.size());
    credb::trusted::PendingBitstreamResponse resp(op_id, *upstream);
    resp.wait();
    upstream->unlock();

    auto encrypted = resp.get_result();
    
    if(encrypted.empty())
    {
        return false;
    }

    bool ok = m_encrypted_io->decrypt_disk(encrypted.data(), encrypted.size(), data);
    
    if(ok)
    {
        return !data.empty();
    }
    else
    {
        return false;
    }
#endif
}

bool Enclave::read_from_local_disk(const std::string &filename,  bitstream &data)
{
    return m_encrypted_io->read_from_disk(filename, data);
}

bool Enclave::dump_everything(const std::string &filename)
{
    bool ok = false;
#ifdef TEST
    (void)filename;
#else
    bitstream metadata;
    m_ledger.dump_metadata(metadata);
    m_buffer_manager.flush_all_pages();

    if(!write_to_disk("___metadata", metadata))
    {
        log_error("Failed to write metadata");
        return false;
    }

    log_info("Disk key is leaked");
    const auto &disk_key = m_encrypted_io->disk_key();
#ifdef FAKE_ENCLAVE
    ok = ::dump_everything(filename.c_str(), reinterpret_cast<const uint8_t *>(disk_key), sizeof(disk_key));
#else
    sgx_status_t res =
    ::dump_everything(&ok, filename.c_str(), reinterpret_cast<const uint8_t *>(disk_key), sizeof(disk_key));
    if(res != SGX_SUCCESS)
    {
        log_error("Failed to dump_everything");
        return false;
    }
#endif

    remove_from_disk("___metadata");

#endif
    return ok;
}

bool Enclave::load_everything(const std::string &filename)
{
    bool ok = false;

#ifdef TEST
    (void)filename;
#else
    m_ledger.unload_everything();
    m_buffer_manager.clear_cache();

    sgx_aes_gcm_128bit_key_t disk_key;
#ifdef FAKE_ENCLAVE
    ok = ::load_everything(filename.c_str(), reinterpret_cast<uint8_t *>(&disk_key), sizeof(disk_key));
#else
    sgx_status_t res =
    ::load_everything(&ok, filename.c_str(), reinterpret_cast<uint8_t *>(&disk_key), sizeof(disk_key));
    if(res != SGX_SUCCESS)
    {
        log_error("Failed to load_everything");
        return false;
    }
#endif

    m_encrypted_io->set_disk_key(disk_key);
    log_info("Disk key has been reloaded");

    bitstream metadata;
    if(!read_from_disk("___metadata", metadata))
    {
        log_error("Failed to read metadata");
        return false;
    }
    remove_from_disk("___metadata");

    m_ledger.load_metadata(metadata);

#endif
    return ok;
}

#ifndef TEST
bool Enclave::set_trigger(const std::string &collection, remote_party_id identifier)
{
    std::lock_guard<std::mutex> lock(m_trigger_mutex);

    auto it = m_triggers.find(collection);

    if(it == m_triggers.end())
    {
        auto upstream = m_peers.find(get_upstream_id());
        upstream->lock();

        taskid_t task_id = 0;
        auto op_id = upstream->get_next_operation_id();

        bitstream req;
        req << MessageType::OperationRequest; 
        req << task_id << op_id << OperationType::SetTrigger << collection;

        upstream->send(req);

        PendingBooleanResponse resp(op_id, *upstream);
        resp.wait();

        upstream->unlock();

        if(!resp.success())
        {
            return false;
        }

        std::unordered_set<remote_party_id> ids = {identifier};
        m_triggers.emplace(collection, std::move(ids));
        
        log_debug("Added upstream trigger");
    }
    else
    {
        it->second.insert(identifier);
    }

    return true;
}

std::unordered_set<remote_party_id> Enclave::get_triggers(const std::string &collection)
{
    std::lock_guard<std::mutex> lock(m_trigger_mutex);

    auto it = m_triggers.find(collection);

    if(it == m_triggers.end())
    {
        return {};
    }
    else
    {
        return it->second;
    }
}

bool Enclave::unset_trigger(const std::string &collection, remote_party_id identifier)
{
    std::lock_guard<std::mutex> lock(m_trigger_mutex);

    auto it = m_triggers.find(collection);

    if(it == m_triggers.end())
    {
        return false;
    }

    auto &set = it->second;
    auto sit = set.find(identifier);

    if(sit == set.end())
    {
        return false;
    }
    
    set.erase(sit);

    if(it->second.empty())
    {
        auto upstream = m_peers.find(get_upstream_id());
        upstream->lock();
 
        taskid_t task_id = 0;
        auto op_id = upstream->get_next_operation_id();

        bitstream req;
        req << MessageType::OperationRequest; 
        req << task_id << op_id << OperationType::UnsetTrigger << collection;

        upstream->send(req);

        PendingBooleanResponse resp(op_id, *upstream);
        resp.wait();

        m_triggers.erase(it);
        upstream->unlock();
        return resp.success();
    }

    return true;
   
}
#endif

void Enclave::remove_from_disk(const std::string &filename)
{
    ::remove_from_disk(filename.c_str());
}

bool Enclave::write_to_disk(const std::string &filename, const bitstream &data)
{
    return m_encrypted_io->write_to_disk(filename, data);
}

} // namespace trusted
} // namespace credb

//// ECALLS

/// Used by the untrusted part to set the enclave's name
credb_status_t credb_init_enclave(const char *name)
{
    credb::trusted::g_enclave = new credb::trusted::Enclave;
    return credb::trusted::g_enclave->init(name);
}

sgx_ec256_public_t credb_get_public_key() { return credb::trusted::g_enclave->public_key(); }

sgx_ec256_public_t credb_get_upstream_public_key()
{
    return credb::trusted::g_enclave->upstream_public_key();
}

void credb_set_upstream(remote_party_id upstream_id)
{
    credb::trusted::g_enclave->set_upstream(upstream_id);
}

void credb_peer_insert_response(remote_party_id peer_id, uint32_t op_id, const uint8_t *data, uint32_t length)
{
#ifdef TEST
    (void)peer_id;
    (void)op_id;
    (void)data;
    (void)length;
#else
    auto opid = static_cast<operation_id_t>(op_id);
    auto peer = credb::trusted::g_enclave->peers().find(peer_id);
    peer->insert_response(opid, data, length);
#endif
}

#ifndef FAKE_ENCLAVE
/// Called by the Python Interpreter
void print_program_output(const std::string &str) { print_info(str.c_str()); }
#endif
