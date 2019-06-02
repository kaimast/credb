/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Client.h"

#include "Enclave.h"
#include "PendingBitstreamResponse.h"
#include "logging.h"
#include "util/EncryptionType.h"

#include <json/json.h>
#include <sgx_tkey_exchange.h>

#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#else
#include "Enclave_t.h"
#include "ias_ra.h"
#endif

namespace credb::trusted
{

Client::Client(Enclave &enclave, int32_t id)
: RemoteParty(enclave, id), m_state(ClientState::Attestation)
{
    log_debug("New client #" + to_string(id));
}

Client::~Client()
{
    //FIXME doesn't work for downstream
    m_enclave.ledger().remove_triggers_for(local_identifier());
}

void Client::handle_message(const uint8_t *data, uint32_t len)
{
    bitstream input;
    decrypt(data, len, input);

    EncryptionType encryption;
    bitstream peeker;
    peeker.assign(data, len, true);
    peeker >> reinterpret_cast<etype_data_t &>(encryption);

    if(encryption == EncryptionType::Attestation)
    {
        // consume the encryption field
        input >> reinterpret_cast<etype_data_t &>(encryption);
    }

    MessageType type;
    input >> reinterpret_cast<mtype_data_t &>(type);

    if(m_state == ClientState::Connected)
    {
        switch(type)
        {
        case MessageType::OperationRequest:
        {
            bitstream output;
            OpContext context(identity(), "");

            unlock();
            handle_op_request(input, output, context);
            lock();

            if(!output.empty())
            {
                send(output);
            }
            break;
        }
        default:
            log_error("Received unexpected message type: " + std::to_string(static_cast<uint8_t>(type)));
        }
    }
    else
    {
        switch(type)
        {
        case MessageType::GroupIdResponse:
            handle_groupid_response(input);
            break;
        case MessageType::AttestationMessage2:
            handle_attestation_message_two(input);
            break;
        case MessageType::AttestationResult:
            handle_attestation_result(input);
            break;
        default:
            log_error("Received unexpected message type: " + std::to_string(static_cast<uint8_t>(type)));
            break;
        }
    }
}

void Client::handle_attestation_result(bitstream &input)
{
    int msg_status[2];

    input >> msg_status[0];
    input >> msg_status[1];

#ifdef FAKE_ENCLAVE
    m_state = ClientState::Connected;
#else

    ias_platform_info_blob_t info_blob;
    sample_mac_t mac;

    input >> info_blob;
    input >> mac;

    // sgx_status_t status;

    // Check the MAC using MK on the attestation result message.
    // The format of the attestation result message is ISV specific.
    // This is a simple form for demonstration. In a real product,
    // the ISV may want to communicate more information.
    /*   auto ret = verify_att_result_mac(m_enclave.identifier(),
               &status,
               m_attestation_context,
               (uint8_t*)&info_blob,
               sizeof(info_blob),
               (uint8_t*)&mac,
               sizeof(mac));

       if((SGX_SUCCESS != ret) ||
          (SGX_SUCCESS != status))
       {
           LOG(ERROR) << "INTEGRITY FAILED - attestation result message MK based cmac failed: " <<
       to_string(ret) << ", " << to_string(status); return;
       }*/

    bool attestation_passed = true;
    // Check the attestation result for pass or fail.
    // Whether attestation passes or fails is a decision made by the ISV Server.
    // When the ISV server decides to trust the enclave, then it will return success.
    // When the ISV server decided to not trust the enclave, then it will return failure.
    if(msg_status[0] != 0 || msg_status[1] != 0)
    {
        log_error("Attestation result message MK based cmac");
        attestation_passed = false;
    }

    // The attestation result message should contain a field for the Platform
    // Info Blob (PIB).  The PIB is returned by attestation server in the attestation report.
    // It is not returned in all cases, but when it is, the ISV app
    // should pass it to the blob analysis API called sgx_report_attestation_status()
    // along with the trust decision from the ISV server.
    // The ISV application will take action based on the update_info.
    // returned in update_info by the API.
    // This call is stubbed out for the sample.
    //
    // sgx_update_info_bit_t update_info;
    // ret = sgx_report_attestation_status(
    //     &p_att_result_msg_body->platform_info_blob,
    //     attestation_passed ? 0 : 1, &update_info);

    // Get the shared secret sent by the server using SK (if attestation
    // passed)
    if(attestation_passed)
    {
        m_state = ClientState::Connected;

        sgx_ra_get_keys(get_attestation_context(), SGX_RA_KEY_SK, &m_sk_key);
    }
    else
    {
        log_error("attestation failed");
    }
#endif
}

bool Client::get_encryption_key(sgx_ec_key_128bit_t **key)
{
    if(m_state != ClientState::Connected)
    {
        return false;
    }

#ifdef FAKE_ENCLAVE
    *key = nullptr;
    return true;
#else

    *key = &m_sk_key;
    return true;
#endif
}

} // namespace credb::trusted
