#pragma once

#include <condition_variable>
#include <yael/network/Socket.h>

#include "util/MessageType.h"
#include "util/Mutex.h"
#include <bitstream.h>

#include "EnclaveHandle.h"

namespace credb
{

enum class AttestationState
{
    TellGroupId,
    WaitingForGroupIdResult,
    WaitingForMsg2,
    Done
};

enum class AttestationMode
{
    OneWay,
    TwoWay_IsInitiator,
    TwoWay_NotInitiator,
};

namespace untrusted
{

class RemoteParty;

class Attestation
{
public:
    Attestation(AttestationMode mode, RemoteParty &remote_party, credb::EnclaveHandle &enclave);
    ~Attestation();

    bool done() const { return m_state == AttestationState::Done; }

    void init();

    void update();

    void queue_groupid_result(bool *result);
    void queue_msg2(sgx_ra_msg2_t *msg2, uint32_t msg2_size);
    void queue_msg3(sgx_ra_msg3_t *msg3);

    void set_done();

    void wait_for_attestation();

    void tell_groupid();

private:
    bool handle_groupid_result(const bool result);
    bool handle_msg2(const sgx_ra_msg2_t *msg2, uint32_t msg2_size);

    AttestationState m_state = AttestationState::TellGroupId;

    bool *m_groupid_result = nullptr;

    sgx_ra_msg2_t *m_msg2 = nullptr;
    uint32_t m_msg2_size = 0;

    const AttestationMode m_mode;

    RemoteParty &m_remote_party;
    credb::EnclaveHandle &m_enclave;

    credb::Mutex m_connection_mutex;
    std::condition_variable_any m_connection_cond;

    sgx_ra_context_t m_attestation_context;
};

} // namespace untrusted
} // namespace credb
