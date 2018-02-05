#pragma once

#ifdef FAKE_ENCLAVE
#include "credb/ucrypto/ucrypto.h"
#include <sgx_ukey_exchange.h>
#else
#include <sgx_key_exchange.h>
#include <sgx_tcrypto.h>
#include <sgx_uae_service.h>
#endif

#include "OpContext.h"
#include "util/OperationType.h"
#include "util/MessageType.h"
#include "util/defines.h"
#include "util/status.h"

#include <memory>
#include <bitstream.h>

namespace credb
{
namespace trusted
{

class TaskManager;
class Enclave;
class Peers;
class TaskManager;
class Ledger;

class RemoteParty : public std::enable_shared_from_this<RemoteParty>
{
public:
    RemoteParty(Enclave &enclave, remote_party_id identifier);
    virtual ~RemoteParty();

    void lock();
    void unlock();
    void wait();
    void notify_all();

    virtual void handle_message(const uint8_t *data, uint32_t len) = 0;

    const std::string &name() const
    {
        if(!m_identity)
            return INVALID_IDENTITY.name;

        return m_identity->name;
    }

    const Identity &identity() const { return *m_identity; }

    void set_attestation_context(sgx_ra_context_t context);

    const sgx_ra_context_t &get_attestation_context() const { return m_attestation_context; }

    remote_party_id local_identifier() const { return m_local_identifier; }

    virtual bool get_encryption_key(sgx_ec_key_128bit_t **key) = 0;

    void send(const bitstream &msg);

    const sgx_ec256_public_t &public_key() const { return m_public_key; }

    virtual bool connected() const = 0;

protected:
    void set_identity(const std::string &name);

    void handle_op_request(bitstream &input, bitstream &output, const OpContext &op_context);

    void handle_execute_request(bitstream &input, const OpContext &op_context, taskid_t task_id, operation_id_t op_id);

    virtual void handle_call_request(bitstream &input, const OpContext &op_context, taskid_t task_id, operation_id_t op_id, bool could_deadlock);
    
    void handle_request_has_object(bitstream &input, const OpContext &op_context, bitstream &output);

    void handle_request_get_object(bitstream &input, const OpContext &op_context, bitstream &output, bool generate_witness);

    void handle_request_upstream_mode(bitstream &input,
                                      const OpContext &op_context,
                                      OperationType op_type,
                                      bitstream &output);
    
    void handle_request_downstream_mode(bitstream &input,
                                        const OpContext &op_context,
                                        OperationType op_type,
                                        bitstream &output);


    void send_attestation_msg(const bitstream &msg);

    void handle_groupid_response(bitstream &input);
    void handle_attestation_message_two(bitstream &input);

    credb_status_t decrypt(const uint8_t *in_data, uint32_t in_len, bitstream &inner);

    /**
     * Encrypts data using the remote parties key
     *
     * @param data
     *     The input data
     * @param encrypted
     *     The output encrypted bitstream
     */
    credb_status_t encrypt(const bitstream &data, bitstream &encrypted);

    Enclave &m_enclave;
    Peers &m_peers;
    Ledger &m_ledger;
    TaskManager &m_task_manager;
    sgx_ec256_public_t m_public_key;

private:
    const remote_party_id m_local_identifier;

    Identity *m_identity;

    sgx_ra_context_t m_attestation_context = -1;
};

} // namespace trusted
} // namespace credb
