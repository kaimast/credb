#pragma once

#include "../Object.h"
#include <cowlang/cow.h>

namespace credb
{
namespace trusted
{
namespace bindings
{

/// Bindings that let a python contract iterate over a ledger's result
class ObjectListIterator : public cow::Generator
{
public:
    ObjectListIterator(cow::MemoryManager &mem, credb::trusted::ObjectListIterator &&it);

    ObjectListIterator(const ObjectListIterator &other) = delete;

    cow::ValuePtr next() override;

    cow::ValuePtr duplicate(cow::MemoryManager &mem) override
    {
         (void)mem;
         return nullptr;
    }

private:
    credb::trusted::ObjectListIterator m_it;
};

} // namespace bindings
} // namespace trusted
} // namespace credb
