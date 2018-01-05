#include "IdentityDatabase.h"

namespace credb
{

Identity &IdentityDatabase::get(const std::string &name)
{
    auto it = m_identities.find(name);

    if(it == m_identities.end())
    {
        // FIXME
        Identity i = { IdentityType::Server, name };
        m_identities.emplace(name, std::move(i));
        it = m_identities.find(name);
    }

    return it->second;
}

} // namespace credb
