#pragma once

#include "Identity.h"
#include <unordered_map>

namespace credb
{

/**
 * Keeps a record of known identities (clients and servers)
 * Eventually this shall communicate with a PKI
 */
class IdentityDatabase
{
public:
    /**
     * Retrieve an identity by name
     */
    Identity& get(const std::string &name, const sgx_ec256_public_t &public_key);

    bool has_identity(const std::string &name) const;
    bool has_identity(const identity_uid_t uid) const;

private:
    // TODO page unneeded data to disk/database
    std::unordered_map<std::string, Identity> m_identities;
};

} // namespace credb
