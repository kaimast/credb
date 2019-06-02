/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <assert.h>
#include <sgx.h>
#include <sgx_urts.h>
#include <stdint.h>
#include <sgx_ukey_exchange.h>
#include <string>
#include <bitstream.h>

#include "Disk.h"
#include "util/defines.h"

namespace credb
{

class EnclaveHandle
{
public:
    EnclaveHandle(std::string name, Disk &disk);
    ~EnclaveHandle();

    EnclaveHandle(const EnclaveHandle &other) = delete;

    sgx_enclave_id_t identifier() const
    {
        if(m_enclave_id == 0)
        {
            LOG(FATAL) << "Enclave not set up yet!";
        }
    
        return m_enclave_id;
    }

    uint32_t groupid() const { return m_extended_groupid; }

    const std::string &name() { return m_name; }

    const sgx_ec256_public_t &public_key() { return m_public_key; }

    const sgx_ec256_public_t &upstream_public_key() { return m_upstream_public_key; }

    bool is_downstream_mode() const { return m_upstream_id != INVALID_REMOTE_PARTY; }

    void init_client(remote_party_id identifier);
    void handle_disconnect(remote_party_id identifier);

    void set_attestation_context(remote_party_id identifier, sgx_ra_context_t context);

    void init_peer(remote_party_id identifier, const std::string &ip, uint16_t port, bool is_initiator);
    void handle_message(const remote_party_id identifier, const uint8_t *data, uint32_t length);

    void set_upstream(remote_party_id upstream_id);
    void peer_insert_response(remote_party_id peer_id, uint32_t op_id, const uint8_t *data, uint32_t length);

    Disk &disk()
    {
        return m_disk;
    }

private:
    uint32_t m_extended_groupid;

    sgx_enclave_id_t m_enclave_id;

#ifndef FAKE_ENCLAVE
    sgx_launch_token_t m_token = { 0 };
#endif

    sgx_ec256_public_t m_public_key;
    sgx_ec256_public_t m_upstream_public_key;

    const std::string m_name;
    remote_party_id m_upstream_id;

    Disk &m_disk;
};

extern EnclaveHandle *g_enclave_handle;

} // namespace credb
