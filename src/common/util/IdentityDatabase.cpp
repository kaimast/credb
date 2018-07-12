#include "IdentityDatabase.h"

namespace credb
{

Identity& IdentityDatabase::get(const std::string &name, const sgx_ec256_public_t &public_key)
{
    if(public_key == INVALID_PUBLIC_KEY)
    {
        throw attestation_error("Not a valid public key!");
    }

    auto it = m_identities.find(name);

    if(it == m_identities.end())
    {
        // FIXME check with PKI
        Identity i = { IdentityType::Server, name, public_key };
        m_identities.emplace(name, std::move(i));
        it = m_identities.find(name);
    }
    else
    {
        if(it->second.public_key() != public_key)
        {
            throw attestation_error("Key's for \"" + name + "\" don't match");
        }
    }

    return it->second;
}

bool IdentityDatabase::has_identity(const std::string &name) const
{
    auto it = m_identities.find(name);
    return it != m_identities.end();
}

bool IdentityDatabase::has_identity(const identity_uid_t uid) const
{
    for(auto &[name, id] : m_identities)
    {
        (void)name;

        if(id.get_unique_id() == uid)
        {
            return true;
        }
    }

    return false;
}


} // namespace credb
