#pragma once

#include "credb/Witness.h"
#include "logging.h"
#include "Enclave.h"

namespace credb
{
namespace trusted
{

inline bool sign_witness(Enclave &enclave, Witness &witness)
{
    sgx_ecc_state_handle_t ecc_state = nullptr;
    auto ret = sgx_ecc256_open_context(&ecc_state);

#ifdef FAKE_ENCLAVE
    if(ret != Status::SUCCESS)
#else
    if(ret != SGX_SUCCESS)
#endif
    {
        log_error("Failed to open ECC context");
        return false;
    }

    ret =
    sgx_ecdsa_sign(witness.data().data(), witness.data().size(), &enclave.private_key(),
                   const_cast<sgx_ec256_signature_t *>(&witness.signature()), // bug in sgx sdk
                   ecc_state);

#ifdef FAKE_ENCLAVE
    if(ret != Status::SUCCESS)
#else
    if(ret != SGX_SUCCESS)
#endif
    {
        log_error("failed to generate signature: " + to_string(ret));
        return false;
    }

    sgx_ecc256_close_context(ecc_state);
    return true;
}

}
}
