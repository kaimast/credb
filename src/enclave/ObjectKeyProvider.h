#pragma once

#include <json/json.h>
#include <set>

#include "LockHandle.h"
#include "ObjectEventHandle.h"
#include "OpContext.h"
#include "ObjectIterator.h"
#include "credb/defines.h"

namespace credb
{
namespace trusted
{

class ObjectKeyProvider
{
public:
    virtual ~ObjectKeyProvider();
    virtual bool get_next_key(std::string &identifier) = 0;
    virtual size_t count_rest() = 0;
};

// TODO: maybe provide better key providers for other indexes and remove this in the future?
class VectorObjectKeyProvider : public ObjectKeyProvider
{
public:
    VectorObjectKeyProvider(std::vector<std::string> &&identifiers) noexcept;
    VectorObjectKeyProvider(VectorObjectKeyProvider &&other) noexcept;
    virtual bool get_next_key(std::string &identifier);
    virtual size_t count_rest();

private:
    std::vector<std::string> m_identifiers;
    std::vector<std::string>::const_iterator m_iterator;
};

} // namespace trusted
} // namespace credb
