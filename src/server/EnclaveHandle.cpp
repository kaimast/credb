#include "EnclaveHandle.h"

#include <glog/logging.h>

#include "util/MessageType.h"
#include "util/defines.h"
#include "util/error.h"

#include "credb/defines.h"

#include "util/remote_attestation_result.h"
#include <sgx_uae_service.h>
#include <sgx_ukey_exchange.h>
#include <sgx_urts.h>

#include <fstream>
#include <unordered_map>

#ifdef FAKE_ENCLAVE
#include "../enclave/Clients.h"
#include "../enclave/Enclave.h"
#include "../enclave/Peers.h"
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

#include "Disk.h"

const char *ENCLAVE_FILE = "enclave.signed.so";

std::string to_string(const sgx_ec256_public_t &key)
{
    std::string result;

    for(size_t i = 0; i < sizeof(key); ++i)
    {
        if(i > 0 && i % 16 == 0)
        {
            result += "\n";
        }
        else if(i > 0 && i % 2 == 0)
        {
            result += ":";
        }

        auto ptr = reinterpret_cast<const uint8_t *>(&key);
        uint8_t val = ptr[i];

        uint8_t u = (val & 0xF0) >> 4;
        uint8_t d = (val & 0x0F);

        result += to_hex(u);
        result += to_hex(d);
    }

    return result;
}

namespace credb
{
EnclaveHandle *g_enclave_handle = nullptr;

#ifdef FAKE_ENCLAVE
EnclaveHandle::EnclaveHandle(const std::string &name)
: m_enclave_id(0), m_name(name), m_upstream_id(INVALID_REMOTE_PARTY)
{
    LOG(INFO) << "Starting fake enclave as '" << m_name << "'";

    credb::trusted::g_enclave = new credb::trusted::Enclave;
    auto ret = credb::trusted::g_enclave->init(name);

    if(ret != CREDB_SUCCESS)
    {
        LOG(FATAL) << "Failed to set up public key for the enclave";
    }

    m_public_key = credb::trusted::g_enclave->public_key();
    LOG(INFO) << "My public key is: \n" << to_string(public_key());
}

EnclaveHandle::~EnclaveHandle()
{
    delete credb::trusted::g_enclave;
}

void EnclaveHandle::handle_peer_message(const remote_party_id identifier, const uint8_t *data, uint32_t length)
{
    trusted::g_enclave->peers().handle_peer_message(identifier, data, length);
}

void EnclaveHandle::set_attestation_context(remote_party_id identifier, sgx_ra_context_t context)
{
    auto remote_party = credb::trusted::g_enclave->remote_parties().find(identifier);

    if(!remote_party)
    {
        return;
    }

    remote_party->set_attestation_context(context);
}

void EnclaveHandle::init_peer(remote_party_id identifier, const std::string &ip, uint16_t port, bool is_initiator)
{
    trusted::g_enclave->peers().init_peer(identifier, ip, port, is_initiator);
}

void EnclaveHandle::init_client(remote_party_id identifier)
{
    trusted::g_enclave->clients().init_client(identifier);
}

void EnclaveHandle::handle_client_disconnect(remote_party_id identifier)
{
    trusted::g_enclave->clients().handle_client_disconnect(identifier);
}

void EnclaveHandle::handle_peer_disconnect(remote_party_id identifier)
{
    trusted::g_enclave->peers().handle_peer_disconnect(identifier);
}

void EnclaveHandle::handle_client_message(remote_party_id identifier, const uint8_t *data, uint32_t length)
{
    trusted::g_enclave->clients().handle_client_message(identifier, data, length);
}

void EnclaveHandle::set_upstream(remote_party_id upstream_id)
{
    m_upstream_id = upstream_id;
    g_disk.clear();
    credb::trusted::g_enclave->set_upstream(upstream_id);
    m_upstream_public_key = credb::trusted::g_enclave->upstream_public_key();
}

void EnclaveHandle::peer_insert_response(remote_party_id peer_id, uint32_t op_id, const uint8_t *data, uint32_t length)
{
    auto opid = static_cast<operation_id_t>(op_id);
    auto peer = credb::trusted::g_enclave->peers().find(peer_id);
    peer->insert_response(opid, data, length);
}

#else

EnclaveHandle::EnclaveHandle(const std::string &name)
: m_enclave_id(0), m_name(name), m_upstream_id(INVALID_REMOTE_PARTY)
{
    int updated = 0;
    g_enclave_handle = this;

    LOG(INFO) << "Starting enclave as '" << m_name << "'";

    auto ret = sgx_create_enclave(ENCLAVE_FILE, SGX_DEBUG_FLAG, &m_token, &updated, &m_enclave_id, nullptr);
    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "Cannot create enclave: " << to_string(ret);
    }

    m_extended_groupid = 0;

    ret = sgx_get_extended_epid_group_id(&m_extended_groupid);
    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "Couldn't get extended EPID group id: " << to_string(ret);
    }

    credb_status_t pret;
    credb_init_enclave(m_enclave_id, &pret, m_name.c_str());

    if(pret != CREDB_SUCCESS)
    {
        LOG(FATAL) << "Failed to set up public key for the enclave";
    }

    assert(m_enclave_id != 0);
    LOG(INFO) << "Enclave set up!";

    ret = credb_get_public_key(m_enclave_id, &m_public_key);

    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "Failed to get public key: " << to_string(ret);
    }

    LOG(INFO) << "My public key is: \n" << to_string(public_key());
}

EnclaveHandle::~EnclaveHandle()
{
    auto ret = sgx_destroy_enclave(m_enclave_id);

    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << ret << ": cant destroy enclave";
    }
    else
    {
        LOG(INFO) << "Enclave destroyed";
    }
}

void EnclaveHandle::handle_peer_message(const remote_party_id identifier, const uint8_t *data, uint32_t length)
{
    credb_status_t status;
    credb_handle_peer_message(m_enclave_id, &status, identifier, data, length);
}

void EnclaveHandle::init_peer(remote_party_id identifier, const std::string &ip, uint16_t port, bool is_initiator)
{
    credb_status_t status = CREDB_SUCCESS;
    credb_init_peer(m_enclave_id, &status, identifier, ip.c_str(), port, is_initiator);

    if(status != CREDB_SUCCESS)
    {
        LOG(ERROR) << "Failed to init peer";
    }
}

void EnclaveHandle::init_client(remote_party_id identifier)
{
    credb_init_client(m_enclave_id, identifier);
}

void EnclaveHandle::handle_client_disconnect(remote_party_id identifier)
{
    credb_handle_client_disconnect(m_enclave_id, identifier);
}

void EnclaveHandle::set_attestation_context(remote_party_id identifier, sgx_ra_context_t context)
{
    credb_set_attestation_context(m_enclave_id, identifier, context);
}

void EnclaveHandle::handle_peer_disconnect(remote_party_id identifier)
{
    credb_status_t status = CREDB_SUCCESS;
    credb_handle_peer_disconnect(m_enclave_id, &status, identifier);

    if(status != CREDB_SUCCESS)
    {
        LOG(ERROR) << "Failed to handle peer disconnect";
    }
}

void EnclaveHandle::handle_client_message(remote_party_id identifier, const uint8_t *data, uint32_t length)
{
    credb_status_t status = CREDB_SUCCESS;
    auto ret = credb_handle_client_message(m_enclave_id, &status, identifier, data, length);

    if(ret != SGX_SUCCESS || status != CREDB_SUCCESS)
    {
        LOG(ERROR) << "Failed to handle message from client #" << to_string(identifier) << ": "
                   << to_string(ret);
    }
}

void EnclaveHandle::set_upstream(remote_party_id upstream_id)
{
    m_upstream_id = upstream_id;
    g_disk.clear();

    sgx_status_t ret = credb_set_upstream(m_enclave_id, upstream_id);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Failed to credb_set_upstream";
    }

    ret = credb_get_upstream_public_key(m_enclave_id, &m_upstream_public_key);
    if(ret != SGX_SUCCESS)
    {
        LOG(FATAL) << "Failed to get upstream public key: " << to_string(ret);
    }
}

void EnclaveHandle::peer_insert_response(remote_party_id peer_id, uint32_t op_id, const uint8_t *data, uint32_t length)
{
    sgx_status_t ret = credb_peer_insert_response(m_enclave_id, peer_id, op_id, data, length);
    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Failed to credb_peer_insert_response" << to_string(ret);    }
}

#endif

} // namespace credb
