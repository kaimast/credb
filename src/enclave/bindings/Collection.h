#pragma once

#include <cowlang/Interpreter.h>

namespace credb
{
namespace trusted
{

class OpContext;
class Enclave;
class Ledger;
class Peers;
class LockHandle;
class ProgramRunner;

namespace bindings
{

class Collection : public cow::Module
{
public:
    Collection(cow::MemoryManager &mem,
               const OpContext &op_context,
               credb::trusted::Ledger &ledger,
               LockHandle &lock_handle,
               const std::string &name);

    cow::ValuePtr get_member(const std::string &name) override;

private:
    const OpContext &m_op_context;
    credb::trusted::Ledger &m_ledger;
    LockHandle &m_lock_handle;
    const std::string m_name;
};

} // namespace bindings
} // namespace trusted
} // namespace credb
