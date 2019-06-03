/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Peer.h"

#include "key_derivation.h"
#include "logging.h"
#include "util/EncryptionType.h"
#include "util/MessageType.h"
#include "util/OperationType.h"
#include "util/defines.h"
#include "util/ecp.h"
#include "util/FunctionCallResult.h"

#include "Enclave.h"
#include "Ledger.h"
#include "TaskManager.h"
#include "util/defines.h"

#include "PendingResponse.h"
#include "ias_ra.h"

#include <sgx_tkey_exchange.h>
#include <cstddef>

#ifdef FAKE_ENCLAVE
#include "../server/FakeEnclave.h"
#else
#endif

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
};

/** FIXME not used yet
#ifndef FAKE_ENCLAVE
//.This.is.supported.extended.epid.group.of.SP..SP.can.support.more.than.one
//.extended.epid.group.with.different.extended.epid.group.id.and.credentials.
static const sample_extended_epid_group g_extended_epid_groups[] = { { 0, ias_enroll, ias_get_sigrl,
                                                                       ias_verify_attestation_evidence } };
#endif**/

// FIXME: this should be set by the IAS server
sample_spid_t g_spid;
// static const sample_extended_epid_group* g_sp_extended_epid_group_id = nullptr; // FIXME: defined
// but not used

namespace credb::trusted
{

Peer::Peer(Enclave &enclave, remote_party_id id, const std::string &hostname, uint16_t port, bool is_initiator)
: RemoteParty(enclave, id), m_is_initiator(is_initiator), m_peer_type(PeerType::Unknown),
  m_hostname(hostname), m_port(port)
{
    if(is_initiator)
    {
        log_debug("New peer (I am initiating) with id " + to_string(id));
    }
    else
    {
        log_debug("New peer (They're initating) with id " + to_string(id));
    }
}

void Peer::set_peer_type(PeerType peer_type)
{
    if(m_peer_type != PeerType::Unknown)
    {
        log_error("m_peer_type != PeerType::Unknown");
    }
    if(peer_type == PeerType::Unknown)
    {
        log_error("peer_type == PeerType::Unknown");
    }

    m_peer_type = peer_type;
    if(peer_type == PeerType::DownstreamServer)
    {
        m_remote_parties.add_downstream_server(local_identifier());
    }
}

bool Peer::get_encryption_key(sgx_ec_key_128bit_t **key)
{
    // We only use encryption once the handshake is done
    if(m_state != PeerState::Connected)
    {
        return false;
    }

#ifdef FAKE_ENCLAVE
    *key = nullptr;
    return true;
#else
    *key = &m_encryption_key;
    return true;
#endif
}

#ifdef FAKE_ENCLAVE
sgx_status_t Peer::handle_attestation_message_one(bitstream &input)
{
    //Not actually processing the input...
    (void)input;

    sgx_ra_msg2_t msg2;
    memset(&msg2, 0, sizeof(msg2));

    bitstream output;
    output << MessageType::AttestationMessage2;
    output << static_cast<uint32_t>(sizeof(msg2) + msg2.sig_rl_size);
    output.write_raw_data(reinterpret_cast<const uint8_t *>(&msg2), sizeof(msg2) + msg2.sig_rl_size);

    this->send_attestation_msg(output);
    return SGX_SUCCESS;
}
#else
sgx_status_t Peer::handle_attestation_message_one(bitstream &input)
{
    sgx_ra_msg1_t msg1;
    sgx_ra_msg2_t msg2;

    input >> msg1;

    bool derive_ret = false;

    /* TODO IAS
    // Check to see if we have registered?
    if (!g_is_sp_registered)
    {
        return SP_UNSUPPORTED_EXTENDED_EPID_GROUP;
    }*/

    sgx_ecc_state_handle_t ecc_state = nullptr;

    // Get the sig_rl from attestation server using GID.
    // GID is Base-16 encoded of EPID GID in little-endian format.
    // In the product, the SP and attesation server uses an established channel for
    // communication./

    // TODO implement this!
    // msg2.sig_rl = nullptr;
    msg2.sig_rl_size = 0;

    // Need to save the peer's public ECCDH key to local storage
    if(memcpy_s(&m_dhke_other_public, sizeof(m_dhke_other_public), &msg1.g_a, sizeof(msg1.g_a)))
    {
        return SGX_ERROR_UNEXPECTED;
    }

    // Generate the Service providers ECCDH key pair.
    auto ret = sgx_ecc256_open_context(&ecc_state);
    if(ret != SGX_SUCCESS)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    ret = sgx_ecc256_create_key_pair(&m_dhke_private, &m_dhke_public, ecc_state);
    if(ret != SGX_SUCCESS)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    // Generate the client/SP shared secret
    sgx_ec256_dh_shared_t dh_key;
    ret = sgx_ecc256_compute_shared_dhkey(&m_dhke_private, (sgx_ec256_public_t *)&m_dhke_other_public,
                                          (sgx_ec256_dh_shared_t *)&dh_key, ecc_state);

    if(ret != SGX_SUCCESS)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    // smk is only needed for msg2->generation.
    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_SMK, &m_smk_key);
    if(derive_ret != true)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    // The rest of the keys are the shared secrets for future communication.
    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_MK, &m_mk_key);
    if(derive_ret != true)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_SK, &m_sk_internal_key);

    if(derive_ret != true)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    derive_ret = derive_key(&dh_key, SAMPLE_DERIVE_KEY_VK, &m_vk_key);
    if(derive_ret != true)
    {
        return SGX_ERROR_UNEXPECTED;
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

    sgx_ec256_public_t gb_ga[2];
    if(memcpy_s(&gb_ga[0], sizeof(gb_ga[0]), &m_dhke_public, sizeof(m_dhke_other_public)) ||
       memcpy_s(&gb_ga[1], sizeof(gb_ga[1]), &m_dhke_other_public, sizeof(m_dhke_public)))
    {
        return SGX_ERROR_UNEXPECTED;
    }

    // Sign gb_ga
    ret = sgx_ecdsa_sign((uint8_t *)&gb_ga, sizeof(gb_ga),
                         (sgx_ec256_private_t *)&m_enclave.private_key(), &msg2.sign_gb_ga, ecc_state);

    if(ret != SGX_SUCCESS)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    // Generate the CMACsmk for gb||SPID||TYPE||KDF_ID||Sigsp(gb,ga)
    std::array<uint8_t, SAMPLE_EC_MAC_SIZE> mac = { 0 };
    uint32_t cmac_size = offsetof(sgx_ra_msg2_t, mac);

    ret = sgx_rijndael128_cmac_msg(&m_smk_key, reinterpret_cast<const uint8_t *>(&msg2), cmac_size, reinterpret_cast<sgx_cmac_128bit_tag_t*>(mac.data()));

    if(ret != SGX_SUCCESS)
    {
        return SGX_ERROR_UNEXPECTED;
    }

    memcpy(reinterpret_cast<void*>(msg2.mac),
            reinterpret_cast<void*>(mac.data()), mac.size());

    // FIXME make sure context is always closed
    if(ecc_state)
    {
        sgx_ecc256_close_context(ecc_state);
    }

    // FIXME: Implement error handling
    bitstream output;
    output << MessageType::AttestationMessage2;
    output << static_cast<uint32_t>(sizeof(msg2) + msg2.sig_rl_size);
    output.write_raw_data(reinterpret_cast<const uint8_t *>(&msg2), sizeof(msg2) + msg2.sig_rl_size);

    this->send_attestation_msg(output);
    return SGX_SUCCESS;
}
#endif

#ifdef FAKE_ENCLAVE
sgx_status_t Peer::handle_attestation_message_three(bitstream &input)
{
    // Not actually processing the input
    (void)input;

    bitstream output;
    output << MessageType::AttestationResult;
    output << 0;
    output << 0;

    this->send_attestation_msg(output);

    if(m_state == PeerState::Attestation_Phase1)
    {
        log_debug("Moving to phase 2");

        m_state = PeerState::Attestation_Phase2;
        attestation_tell_groupid(local_identifier());
    }
    else
    {
        set_connected();
    }
    return SGX_SUCCESS;
}
#else
sgx_status_t Peer::handle_attestation_message_three(bitstream &input)
{
    sgx_sha_state_handle_t sha_handle = nullptr;
    sample_report_data_t report_data = { 0 };

    ias_att_report_t attestation_report;

    sgx_ra_msg3_t *msg3 = nullptr;
    uint32_t msg3_size = 0;
    input >> msg3_size;
    input.read_raw_data(reinterpret_cast<uint8_t **>(&msg3), msg3_size);

    // Compare g_a in message 3 with local g_a.
    if(memcmp(&m_dhke_other_public, &msg3->g_a, sizeof(sgx_ec256_public_t)) != 0)
    {
        // LOG(ERROR) << "Error, g_a is not same";
        return SGX_ERROR_UNEXPECTED; // SP_PROTOCOL_ERROR;
    }

    // Make sure that msg3_size is bigger than sample_mac_t.
    uint32_t mac_size = msg3_size - sizeof(sample_mac_t);
    auto msg_cmaced = reinterpret_cast<const uint8_t *>(msg3);
    msg_cmaced += sizeof(sample_mac_t);

    // Verify the message mac using SMK
    sgx_cmac_128bit_tag_t mac = { 0 };
    auto ret = sgx_rijndael128_cmac_msg(&m_smk_key, msg_cmaced, mac_size, &mac);

    if(ret != SGX_SUCCESS)
    {
        log_error("cmac fail");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
    }

    // In real implementation, should use a time safe version of memcmp here,
    // in order to avoid side channel attack.
    if(memcmp(&msg3->mac[0], &mac[0], sizeof(mac)) != 0)
    {
        log_error("verify cmac fail");
        return SGX_ERROR_UNEXPECTED; // SP_INTEGRITY_FAILED;
    }

    if(memcpy_s(&m_ps_sec_prop, sizeof(m_ps_sec_prop), &msg3->ps_sec_prop, sizeof(msg3->ps_sec_prop)) != 0)
    {
        log_error("memcpy failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
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
    ret = sgx_sha256_init(&sha_handle);
    if(ret != SGX_SUCCESS)
    {
        log_error("init hash failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
    }

    ret = sgx_sha256_update((uint8_t *)&(m_dhke_other_public), sizeof(m_dhke_other_public), sha_handle);

    if(ret != SGX_SUCCESS)
    {
        log_error("Update hash failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
    }
    ret = sgx_sha256_update(reinterpret_cast<uint8_t *>(&m_dhke_public), sizeof(m_dhke_public), sha_handle);
    if(ret != SGX_SUCCESS)
    {
        log_error("Update hash failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
    }

    ret = sgx_sha256_update(reinterpret_cast<uint8_t *>(&m_vk_key), sizeof(m_vk_key), sha_handle);

    if(ret != SGX_SUCCESS)
    {
        log_error("Update hash failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
    }

    ret = sgx_sha256_get_hash(sha_handle, (sgx_sha256_hash_t *)&report_data);

    if(ret != SGX_SUCCESS)
    {
        log_error("Get hash failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTERNAL_ERROR;
    }


    if(memcmp((uint8_t *)&report_data, (uint8_t *)&(p_quote->report_body.report_data), sizeof(report_data)) != 0)
    {
        log_error("Get hash failed");
        return SGX_ERROR_UNEXPECTED; // SP_INTEGRITY_FAILED;
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
        return SGX_ERROR_UNEXPECTED; // SP_IAS_FAILED;
    }

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

    //  if((IAS_QUOTE_OK == attestation_report.status) &&
    //        (IAS_PSE_OK == attestation_report.pse_status) &&
    //      (isv_policy_passed == true))
    sgx_sha256_close(sha_handle);

    if(isv_policy_passed && ret == SGX_SUCCESS)
    {
        bitstream output;
        output << MessageType::AttestationResult;
        output << 0;
        output << 0;

        output << info_blob;
        output << blob_mac;

        this->send_attestation_msg(output);

        if(m_state == PeerState::Attestation_Phase1)
        {
            log_debug("Moving to phase 2");

            m_state = PeerState::Attestation_Phase2;
            attestation_tell_groupid(local_identifier());
        }
        else
        {
            set_connected();
        }

        return SGX_SUCCESS;
    }
    else
    {
        return SGX_ERROR_UNEXPECTED;
    }
}
#endif

void Peer::handle_message(const uint8_t *data, uint32_t len)
{
    bitstream input;
    decrypt(data, len, input);

    // FIXME: this is ugly, EncryptionType::Attestation is actually plain text
    EncryptionType encryption;
    bitstream peeker;
    peeker.assign(data, len, true);
    peeker >> reinterpret_cast<etype_data_t &>(encryption);

    if(encryption == EncryptionType::Attestation)
    {
        // consume the encryption field
        input >> reinterpret_cast<etype_data_t &>(encryption);
        log_debug("Received attestation message from peer");
    }
    else
    {
        assert(encryption == EncryptionType::Encrypted);
        // log_debug("Received encrypted message from peer");
    }

    if(input.size() < sizeof(mtype_data_t))
    {
        log_error("Received invalid encrypted message from peer");
        return;
    }

    MessageType type;
    input >> reinterpret_cast<mtype_data_t &>(type);

    if(m_state == PeerState::Connected)
    {
        switch(type)
        {
        case MessageType::OperationRequest:
        {
            bitstream output;

            if(!has_identity())
            {
                log_error("Invalid state: No identity set up");
            }

            unlock();
            OpContext op_context(identity(), "");
            handle_op_request(input, output, op_context);
            lock();

            if(!output.empty())
            {
                send(output);
            }
            break;
        }
        case MessageType::NotifyTrigger:
        {
            log_debug("Forwarding trigger notify");

            //FIXME don't just assume we are upstream.
            std::string collection;
            input >> collection;

            auto set = m_enclave.get_triggers(collection);

            bitstream bs;
            bs << MessageType::NotifyTrigger << collection;

            auto &parties = m_enclave.remote_parties();

            for(auto i: set)
            {
                auto p = parties.find<RemoteParty>(i);
                
                if(p)
                {
                    p->send(bs);
                }
            }

            break;
        }
        case MessageType::PushIndexUpdate:
        {
            unlock();
            bitstream changes;
            shard_id_t shard;
            page_no_t block_page_no;
            uint16_t block_size;

            input >> changes >> shard >> block_page_no >> block_size;
            
            m_ledger.put_object_index_from_upstream(changes, shard, block_page_no, block_size);
            lock();
            break;
        }
        case MessageType::OperationResponse:
            handle_op_response(input);
            break;
        case MessageType::ForwardedOperationRequest:
            handle_op_forwarded_request(input);
            break;
        default:
            log_error("Received unknown request");
        }
        return;
    }

    if(m_state == PeerState::Attestation_Phase1)
    {
        log_debug("Received message in phase 1");
    }
    else
    {
        log_debug("Received message in phase 2");
    }

    if((m_is_initiator && m_state == PeerState::Attestation_Phase1) ||
       (!m_is_initiator && m_state == PeerState::Attestation_Phase2))
    {
        switch(type)
        {
        case MessageType::GroupIdResponse:
        {
            handle_groupid_response(input);
            break;
        }
        case MessageType::AttestationMessage2:
            handle_attestation_message_two(input);
            break;
        case MessageType::AttestationResult:
            handle_attestation_result(input);
            break;
        case MessageType::OperationResponse:
        case MessageType::OperationRequest:
            log_error("Cannot handle operation request/response (yet)");
            break;
        case MessageType::TellGroupId:
            log_error("Received unexpected message type");
            break;
        default:
            log_error("Received unknown message from peer");
            break;
        }
    }
    else
    {
        switch(type)
        {
        case MessageType::TellGroupId:
        {
            //            std::string name;

            input >> m_group_id;
            // FIXME should we verify name / pk here?
            //            input >> name;
            //            input >> m_public_key;

            log_debug("Received valid groupid");

            // TODO contact Intel Attestation Service

            bitstream output;
            output << static_cast<mtype_data_t>(MessageType::GroupIdResponse);
            output << true;
            output << m_enclave.name();
            output << m_enclave.public_key();

            send_attestation_msg(output);
            break;
        }
        case MessageType::AttestationMessage1:
        {
            auto ret = handle_attestation_message_one(input);
            if(ret != SGX_SUCCESS)
            {
                log_error("failed to handle msg1");
            }
            break;
        }
        case MessageType::AttestationMessage3:
        {
            auto res = handle_attestation_message_three(input);

            if(res != SGX_SUCCESS)
            {
                log_error("failed to handle message 3");
            }
            break;
        }
        case MessageType::GroupIdResponse:
            log_error("Received unkexpected message");
            break;
        default:
            log_error("Received unknown message type");
            return;
        }
    }
}

#ifdef FAKE_ENCLAVE
void Peer::handle_attestation_result(bitstream &input)
{
    log_debug("Received attestation result");

    int msg_status[2];

    input >> msg_status[0];
    input >> msg_status[1];

    if(m_state == PeerState::Attestation_Phase2)
    {
        log_debug("Remote attestation success!");
        set_connected();
    }
    else
    {
        log_debug("Remote attestation phase one done!");
        m_state = PeerState::Attestation_Phase2;
    }
}
#else
void Peer::handle_attestation_result(bitstream &input)
{
    log_debug("Received attestation result");

    int msg_status[2];

    input >> msg_status[0];
    input >> msg_status[1];

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
        if(m_state == PeerState::Attestation_Phase2)
        {
            log_debug("Remote attestation success!");
            set_connected();
        }
        else
        {
            log_debug("Remote attestation phase one done!");
            m_state = PeerState::Attestation_Phase2;
        }
    }
    else
    {
        log_error("attestation failed");
    }
}
#endif

void Peer::set_connected()
{
    m_state = PeerState::Connected;
    //m_remote_parties.set_name(local_identifier(), name());

    sgx_ec_key_128bit_t external_key;

#ifdef FAKE_ENCLAVE
    memset(external_key, 0, sizeof(external_key));
#else
    sgx_ra_get_keys(get_attestation_context(), SGX_RA_KEY_SK, &external_key);

    auto out = reinterpret_cast<uint8_t *>(m_encryption_key);
    auto in1 = reinterpret_cast<const uint8_t *>(external_key);
    auto in2 = reinterpret_cast<const uint8_t *>(m_sk_internal_key);

    for(uint32_t i = 0; i < 16; ++i)
    {
        out[i] = in1[i] ^ in2[i];
    }
#endif

    attestation_notify_done(local_identifier());
}

void Peer::handle_op_forwarded_request(bitstream &input)
{
    operation_id_t op_id;
    taskid_t task_id;

    bitstream forwarded_input;
    input >> task_id >> op_id;
    input >> forwarded_input;

    bitstream output_to_forward;
    MessageType ignore;
    forwarded_input >> reinterpret_cast<mtype_data_t &>(ignore);

    unlock();
    // FIXME use client identity
    OpContext op_context(identity(), "");
    RemoteParty::handle_op_request(forwarded_input, output_to_forward, op_context);
    lock();

    bitstream output;
    output << MessageType::OperationResponse;
    output << task_id << op_id;
    output << output_to_forward;

    send(output);
    // log_debug("handle_op_forwarded_request, peer_id=" + std::to_string(local_identifier()) + "
    // op_id=" + std::to_string(op_id) + " done");
}

operation_id_t Peer::call(const std::string &collection,
                          taskid_t task_id,
                          const std::string &key,
                          const std::string &path,
                          const std::vector<std::string> &args,
                          identity_uid_t transaction_root,
                          transaction_id_t transaction_id)
{
    auto op_id = get_next_operation_id();

    auto bstream = generate_op_request(task_id, op_id, OperationType::CallProgram);
    bstream << collection << key << path << args;
    bstream << transaction_root << transaction_id;

    send(bstream);

    return op_id;
}

void Peer::handle_op_response(bitstream &input)
{
    operation_id_t op_id;
    taskid_t task_id;

    input >> task_id >> op_id;

    auto pos = input.pos();

    auto copy = new bitstream(std::move(input));
    copy->move_to(pos);

    m_responses.insert({ op_id, copy });

    if(task_id == 0)
    {
        notify_all();
    }
    else
    {
        try {
            auto t = m_task_manager.get_task(task_id);
            unlock();
            t->handle_op_response();
            lock();
        } catch(std::runtime_error &e) {
            log_warning(e.what());
        }
    }
}

bitstream *Peer::receive_response(operation_id_t op_id, bool wait)
{
    auto it = m_responses.find(op_id);

    while(wait && it == m_responses.end())
    {
        this->wait();
        it = m_responses.find(op_id);
    }

    if(it == m_responses.end())
    {
        return nullptr;
    }
    else
    {
        auto bstream = it->second;
        m_responses.erase(it);

        return bstream;
    }
}

void Peer::insert_response(operation_id_t op_id, const uint8_t *data, uint32_t length)
{
    auto bstream = new bitstream(data, length);
    m_responses.insert({ op_id, bstream });
}

void Peer::handle_call_request(bitstream &input, const OpContext &op_context, taskid_t task_id, operation_id_t op_id)
{
    RemoteParty::handle_call_request(input, op_context, task_id, op_id);
}

} // namespace credb::trusted
