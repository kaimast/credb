#pragma once

#include "RemoteParty.h"
#include <sgx_tkey_exchange.h>

namespace credb
{
namespace trusted
{

enum class ClientState : uint8_t
{
    Attestation = 0,
    Connected
};

class Client : public RemoteParty
{
public:
    Client(Enclave &enclave, int32_t id);
    ~Client();

    Client(const Client &other) = delete;

    void handle_message(const uint8_t *data, uint32_t len) override;

    bool get_encryption_key(sgx_ec_key_128bit_t **key) override;

    const sgx_ec256_public_t &public_key() const { return m_public_key; }

    bool connected() const override { return m_state == ClientState::Connected; }

private:
    void handle_attestation_result(bitstream &input);

    ClientState m_state;

#ifndef FAKE_ENCLAVE
    sgx_ec_key_128bit_t m_sk_key; // Shared secret key for encryption
#endif
};

} // namespace trusted
} // namespace credb
