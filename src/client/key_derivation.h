#pragma once

#include "util/keys.h"
#include <cstring>
#include <sgx_key_exchange.h>

// Derive key from shared key and key id.
// key id should be sample_derive_key_type_t.
inline bool derive_key(const sgx_ec256_dh_shared_t *p_shared_key, uint8_t key_id, sgx_ec_key_128bit_t *derived_key)
{
    uint8_t cmac_key[MAC_KEY_SIZE];
    sgx_ec_key_128bit_t key_derive_key;

    memset(&cmac_key, 0, MAC_KEY_SIZE);

    auto ret = sgx_rijndael128_cmac_msg((sgx_cmac_128bit_key_t *)&cmac_key, (uint8_t *)p_shared_key,
                                        sizeof(sgx_ec256_dh_shared_t), (sgx_cmac_128bit_tag_t *)&key_derive_key);

    if(ret != SGX_SUCCESS)
    {
        // LOG(ERROR) << "CMAC failed";
        // memset here can be optimized away by compiler, so please use memset_s on
        // windows for production code and similar functions on other OSes.
        memset(&key_derive_key, 0, sizeof(key_derive_key));
        return false;
    }

    const char *label = NULL;
    uint32_t label_length = 0;
    switch(key_id)
    {
    case SAMPLE_DERIVE_KEY_SMK:
        label = str_SMK;
        label_length = sizeof(str_SMK) - 1;
        break;
    case SAMPLE_DERIVE_KEY_SK:
        label = str_SK;
        label_length = sizeof(str_SK) - 1;
        break;
    case SAMPLE_DERIVE_KEY_MK:
        label = str_MK;
        label_length = sizeof(str_MK) - 1;
        break;
    case SAMPLE_DERIVE_KEY_VK:
        label = str_VK;
        label_length = sizeof(str_VK) - 1;
        break;
    default:
        // memset here can be optimized away by compiler, so please use memset_s on
        // windows for production code and similar functions on other OSes.
        memset(&key_derive_key, 0, sizeof(key_derive_key));
        return false;
        break;
    }
    /* derivation_buffer = counter(0x01) || label || 0x00 || output_key_len(0x0080) */
    uint32_t derivation_buffer_length = EC_DERIVATION_BUFFER_SIZE(label_length);
    uint8_t *p_derivation_buffer = new uint8_t[derivation_buffer_length];
    if(p_derivation_buffer == NULL)
    {
        // memset here can be optimized away by compiler, so please use memset_s on
        // windows for production code and similar functions on other OSes.
        memset(&key_derive_key, 0, sizeof(key_derive_key));
        return false;
    }
    memset(p_derivation_buffer, 0, derivation_buffer_length);

    /*counter = 0x01 */
    p_derivation_buffer[0] = 0x01;
    /*label*/
    memcpy(&p_derivation_buffer[1], label, label_length);
    /*output_key_len=0x0080*/
    uint16_t *key_len = (uint16_t *)(&(p_derivation_buffer[derivation_buffer_length - 2]));
    *key_len = 0x0080;


    ret = sgx_rijndael128_cmac_msg((sgx_cmac_128bit_key_t *)&key_derive_key, p_derivation_buffer,
                                   derivation_buffer_length, (sgx_cmac_128bit_tag_t *)derived_key);

    delete[] p_derivation_buffer;

    // memset here can be optimized away by compiler, so please use memset_s on
    // windows for production code and similar functions on other OSes.
    memset(&key_derive_key, 0, sizeof(key_derive_key));

    if(ret != SGX_SUCCESS)
    {
        // LOG(FATAL) << "CMAC failed";
        return false;
    }
    else
        return true;
}
