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
class ProgramRunner;
class LockHandle;

namespace bindings
{

/**
 * @brief Python bindings for the ledger itself
 */
class Database : public cow::Module
{
public:
    Database(cow::MemoryManager &mem,
             const OpContext &op_context,
             credb::trusted::Ledger &ledger,
             credb::trusted::Enclave &enclave,
             ProgramRunner *runner,
             LockHandle &lock_handle);

    cow::ValuePtr get_member(const std::string &name) override;

private:
    const OpContext &m_op_context;
    credb::trusted::Ledger &m_ledger;
    LockHandle &m_lock_handle;

#ifndef TEST
    ProgramRunner *m_runner;
    credb::trusted::Enclave &m_enclave;
    credb::trusted::Peers &m_peers;
#endif
};

} // namespace bindings
} // namespace trusted
} // namespace credb
