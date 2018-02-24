#include "ObjectKeyProvider.h"
#include "Block.h"
#include "Ledger.h"
#include "LockHandle.h"
#include "logging.h"
#include "BufferManager.h"

namespace credb
{
namespace trusted
{

ObjectKeyProvider::~ObjectKeyProvider() = default;

VectorObjectKeyProvider::VectorObjectKeyProvider(std::vector<std::string> &&identifiers) noexcept
: m_identifiers(std::move(identifiers)), m_iterator(m_identifiers.cbegin())
{
}

VectorObjectKeyProvider::VectorObjectKeyProvider(VectorObjectKeyProvider &&other) noexcept
: m_identifiers(other.m_identifiers), m_iterator(other.m_iterator)
{
}

bool VectorObjectKeyProvider::get_next_key(std::string &identifier)
{
    if(m_iterator == m_identifiers.end())
    {
        return false;
    }

    identifier = *m_iterator;
    ++m_iterator;
    return true;
}

size_t VectorObjectKeyProvider::count_rest()
{
    return m_identifiers.size() - (m_identifiers.cbegin() - m_iterator);
}


//------ ObjectIterator


} // namespace trusted
} // namespace credb
