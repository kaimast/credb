#pragma once

#include <cowlang/Interpreter.h>

namespace credb
{
namespace trusted
{

class OpContext;
class Enclave;
class Ledger;
class RemoteParties;
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

    virtual bool is_transaction() const
    {
        return false;
    }

    cow::ValuePtr get_member(const std::string &name) override;

#ifndef IS_TEST
    bool has_program_runner() const
    {
        return m_runner != nullptr;
    }

    ProgramRunner& runner()
    {
        return *m_runner;
    }
#endif

protected:
    const OpContext &m_op_context;
    credb::trusted::Ledger &m_ledger;
    LockHandle &m_lock_handle;

#ifndef IS_TEST
    ProgramRunner *m_runner;
    credb::trusted::Enclave &m_enclave;
    credb::trusted::RemoteParties &m_remote_parties;
#endif
};

} // namespace bindings
} // namespace trusted
} // namespace credb
