#pragma once

#include <cowlang/Interpreter.h>

namespace credb
{
namespace trusted
{

class OpContext;
class Enclave;
class Ledger;
class LockHandle;

namespace bindings
{

/// Python bindings for a particular object
/// Exposed through the "self" module
class Object : public cow::Module
{
public:
    Object(cow::MemoryManager &mem,
           const credb::trusted::OpContext &op_context,
           Ledger &ledger,
           const std::string &collection,
           const std::string &key,
           LockHandle &lock_handle);

    cow::ValuePtr get_member(const std::string &name) override;

private:
    const credb::trusted::OpContext &m_op_context;
    credb::trusted::Ledger &m_ledger;
    const std::string m_collection;
    const std::string m_key;
    LockHandle &m_lock_handle;
};
} // namespace bindings
} // namespace trusted
} // namespace credb
