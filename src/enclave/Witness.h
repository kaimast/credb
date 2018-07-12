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

    if(ret != SGX_SUCCESS)
    {
        log_error("Failed to open ECC context");
        return false;
    }

    bool success = false;
    
    // Work around crappy const correctness in SGx SDK
    auto pkey = const_cast<sgx_ec256_private_t*>(&enclave.private_key());
    auto sig =  const_cast<sgx_ec256_signature_t *>(&witness.signature());

    ret = sgx_ecdsa_sign(witness.data().data(), witness.data().size(), pkey, sig, ecc_state);

    if(ret == SGX_SUCCESS)
    {
        success = true;
    }
    else
    {
        log_error("failed to generate signature: " + to_string(ret));
    }

    sgx_ecc256_close_context(ecc_state);
    return success;
}

}
}
