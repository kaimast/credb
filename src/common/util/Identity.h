#pragma once

#ifdef FAKE_ENCLAVE
#include "credb/ucrypto/ucrypto.h"
#else
#include <sgx_tcrypto.h>
#endif

#include <stdint.h>
#include <string>
#include <cstring>
#include <stdexcept>

#include "util/hash.h"

namespace credb
{

typedef uint64_t identity_uid_t;
constexpr identity_uid_t INVALID_UID = 0;

class attestation_error : public std::exception
{
    std::string m_what;

public:
    attestation_error(const std::string &what) : m_what(what) {}

    virtual const char *what() const throw() { return m_what.c_str(); }
};

enum class IdentityType : uint8_t
{
    Server,
    Client
};

inline constexpr bool runs_in_tee(IdentityType type) { return type == IdentityType::Server; }

struct Identity
{
public:
    Identity(IdentityType type, const std::string &name, const sgx_ec256_public_t& public_key)
            : m_type(type), m_name(name), m_public_key(public_key) {}

    Identity(Identity &&other)
        : m_type(other.m_type), m_name(std::move(other.m_name)), m_public_key(other.m_public_key) {}

    std::string to_string() const
    {
        if(type() == IdentityType::Server)
        {
            return "server://" + name();
        }
        else if(type() == IdentityType::Client)
        {
            return "client://" + name();
        }
        else
        {
            throw std::runtime_error("Unknown identity type");
        }
    }

    /**
     * Get the unique identifier of this identity
     * This will be generated from the public key
     */
    identity_uid_t get_unique_id() const
    {
        return hash(m_public_key);
    }

    IdentityType type() const
    {
        return m_type;
    }

    bool valid() const
    {
        return !m_name.empty();
    }

    /**
     * The unique human-readable name of this identity
     */
    const std::string& name() const
    {
        return m_name;
    }

    const sgx_ec256_public_t& public_key() const
    {
        return m_public_key;
    }

private:
    const IdentityType m_type;
    const std::string m_name;
    const sgx_ec256_public_t m_public_key;
};

inline bool operator==(const Identity &first, const Identity &second)
{
    return first.name() == second.name();
}

inline bool operator!=(const Identity &first, const Identity &second)
{
    return !(first == second);
}

static const sgx_ec256_public_t INVALID_PUBLIC_KEY = { {}, {} };
static const Identity INVALID_IDENTITY = { IdentityType::Server, "", INVALID_PUBLIC_KEY};

inline bool operator==(const sgx_ec256_public_t &first, const sgx_ec256_public_t &second)
{
    return memcmp(&first, &second, sizeof(sgx_ec256_public_t)) == 0;
}

inline bool operator!=(const sgx_ec256_public_t &first, const sgx_ec256_public_t &second)
{
    return !(first == second);
}




} // namespace credb
