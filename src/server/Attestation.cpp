/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Attestation.h"

#include <glog/logging.h>
#include <sgx_uae_service.h>
#include <sgx_ukey_exchange.h>
#include <stdbitstream.h>

#ifdef FAKE_ENCLAVE
#include "FakeEnclave.h"
#else
#include "Enclave_u.h"
#endif

#include "RemoteParties.h"
#include "RemoteParty.h"

#include "util/EncryptionType.h"
#include "util/error.h"
#include "util/remote_attestation_result.h"

#include <chrono>
#include <thread>
#include <unordered_map>

namespace credb
{
namespace untrusted
{

#ifndef FAKE_ENCLAVE
// How long to sleep between retires (in ms)
const uint32_t RETRY_SLEEP = 100;

const uint32_t NUM_RETRIES = 100;
#endif

Attestation::Attestation(AttestationMode mode, RemoteParty &remote_party, credb::EnclaveHandle &enclave)
: m_mode(mode), m_remote_party(remote_party), m_enclave(enclave)
{
}

Attestation::~Attestation() = default;

void Attestation::init()
{
    if(m_mode == AttestationMode::TwoWay_IsInitiator || m_mode == AttestationMode::OneWay)
    {
        assert(m_state == AttestationState::TellGroupId);
        tell_groupid();
    }
}

void Attestation::update()
{
    bool res = true;

    switch(m_state)
    {
    case AttestationState::Done:
        // Nothing to do
        break;
    case AttestationState::WaitingForGroupIdResult:
        if(m_groupid_result != nullptr)
        {
            res = handle_groupid_result(*m_groupid_result);
            delete m_groupid_result;
            m_groupid_result = nullptr;
        }
        break;
    case AttestationState::WaitingForMsg2:
        if(m_msg2 != nullptr)
        {
            res = handle_msg2(m_msg2, m_msg2_size);
            free(m_msg2);
            m_msg2 = nullptr;
        }
        break;
    case AttestationState::TellGroupId:
        // Handled inside the enclave
        break;
    default:
        LOG(ERROR) << "Unknown attestation state";
        res = false;
    }

    if(!res)
    {
        LOG(INFO) << "Attestation failed. Disconnecting client...";
        m_remote_party.disconnect();
    }
}

void Attestation::wait_for_attestation()
{
    std::unique_lock<credb::Mutex> lock(m_connection_mutex);

    // FIXME add timeout
    while(!done())
    {
        m_connection_cond.wait(lock);
    }
}

void Attestation::set_done()
{
    assert(m_state != AttestationState::Done);
    LOG(INFO) << "Attestation done for remote party #" << m_remote_party.identifier();

    std::unique_lock<credb::Mutex> lock(m_connection_mutex);
    m_state = AttestationState::Done;
    m_connection_cond.notify_all();
}

bool Attestation::handle_groupid_result(const bool result)
{
    if(!result)
    {
        return false;
    }

    credb_status_t status;
    sgx_ra_msg1_t msg1;

#ifdef FAKE_ENCLAVE
    auto ret = SGX_SUCCESS;
    status = CREDB_SUCCESS;
#else
    auto ret = credb_ra_init(m_enclave.identifier(), &status, m_remote_party.identifier(), &m_attestation_context);
#endif

    if(ret != SGX_SUCCESS || status != CREDB_SUCCESS)
    {
        LOG(ERROR) << "Could not get attestation context";
        return false;
    }

#ifdef FAKE_ENCLAVE
    ret = SGX_SUCCESS;
#else
    uint32_t i = 0;

    do
    {
        ret = sgx_ra_get_msg1(m_attestation_context, m_enclave.identifier(), sgx_ra_get_ga, &msg1);
        i += 1;
    } while(ret != SGX_SUCCESS && i < NUM_RETRIES);
// FIXME?    } while(ret == SGX_ERROR_BUSY && i < NUM_RETRIES);
#endif

    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Could not get message 1: " << to_string(ret);
        return false;
    }

    DLOG(INFO) << "Sending message 1";

    bitstream output;
    output << static_cast<etype_data_t>(
    EncryptionType::Attestation); // FIXME: actually, this is plain text
    output << static_cast<mtype_data_t>(MessageType::AttestationMessage1);
    output << msg1;

    m_remote_party.send(output.data(), output.size());
    m_state = AttestationState::WaitingForMsg2;
    return true;
}

bool Attestation::handle_msg2(const sgx_ra_msg2_t *msg2, uint32_t msg2_size)
{
    uint32_t msg3_size = 0;
    sgx_ra_msg3_t *msg3 = nullptr;

#ifdef FAKE_ENCLAVE
    // not actually processing the message
    (void)msg2;
    (void)msg2_size;

    auto ret = SGX_SUCCESS;

    bitstream output;
    output << EncryptionType::Attestation;
    output << MessageType::AttestationMessage3;
    output << 0;

    m_remote_party.send(output.data(), output.size());
    m_remote_party.set_attestation_context(m_attestation_context);

    return true;

#else
    auto ret = SGX_ERROR_UNEXPECTED;
    uint32_t i = 0;

    do
    {
        if(i > 0)
        {
            DLOG(INFO) << "Retrying ra_proc_msg2...";
            std::chrono::milliseconds dur(RETRY_SLEEP);
            std::this_thread::sleep_for(dur);
        }

        ret = sgx_ra_proc_msg2(m_attestation_context, m_enclave.identifier(), sgx_ra_proc_msg2_trusted,
                               sgx_ra_get_msg3_trusted, msg2, msg2_size, &msg3, &msg3_size);
        i += 1;
    } while(ret != SGX_SUCCESS && i < NUM_RETRIES);
// FIXME?    } while(ret == SGX_ERROR_BUSY && i < NUM_RETRIES);
#endif

    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Could not process message 2: " << to_string(ret);
        return false;
    }

    if(msg3 == nullptr)
    {
        LOG(ERROR) << "call sgx_ra_proc_msg2 failed: busy?";
        ret = SGX_ERROR_BUSY;
    }

    if(ret != SGX_SUCCESS)
    {
        LOG(ERROR) << "Call sgx_ra_proc_msg2 failed: " << to_string(ret);
        free(msg3);
        return false;
    }
    else
    {
        DLOG(INFO) << "Processed message 2 and generated message 3 successfully";

        bitstream output;
        output << EncryptionType::Attestation;
        output << MessageType::AttestationMessage3;
        output << msg3_size;

        output.write_raw_data(reinterpret_cast<const uint8_t *>(msg3), msg3_size);

        m_remote_party.send(output.data(), output.size());
        m_remote_party.set_attestation_context(m_attestation_context);

        if(m_mode == AttestationMode::TwoWay_IsInitiator)
        {
            DLOG(INFO) << "Done with attestation phase 1";
            m_state = AttestationState::TellGroupId;
        }

        free(msg3);
        return true;
    }
}

void Attestation::tell_groupid()
{
    DLOG(INFO) << "Sending GroupID";

    bitstream bstream;
    bstream << EncryptionType::Attestation;
    bstream << MessageType::TellGroupId;
    bstream << m_enclave.groupid();
    bstream << m_enclave.name();
    bstream << m_enclave.public_key();
    bstream << m_enclave.is_downstream_mode();
    if(m_enclave.is_downstream_mode())
    {
        bstream << m_enclave.upstream_public_key();
    }

    m_remote_party.send(bstream.data(), bstream.size());
    m_state = AttestationState::WaitingForGroupIdResult;
}

void Attestation::queue_groupid_result(bool *result)
{
    m_groupid_result = result;
}

void Attestation::queue_msg2(sgx_ra_msg2_t *msg2, uint32_t msg2_size)
{
    assert(m_msg2 == nullptr);
    m_msg2 = msg2;
    m_msg2_size = msg2_size;
}
}
} // namespace credb
