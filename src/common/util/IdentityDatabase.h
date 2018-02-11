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
    Identity& get(const std::string &name);

private:
    // TODO page unneeded data to disk/database
    std::unordered_map<std::string, Identity> m_identities;
};

} // namespace credb
