/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "ObjectKeyProvider.h"
#include "Block.h"
#include "Ledger.h"
#include "LockHandle.h"
#include "logging.h"
#include "BufferManager.h"

namespace credb::trusted
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

} // namespace credb::trusted
