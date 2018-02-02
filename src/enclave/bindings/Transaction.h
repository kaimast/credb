#pragma once

#include <cowlang/Interpreter.h>

#include "../Transaction.h"

namespace credb
{
namespace trusted
{

class OpContext;
class LockHandle;
class Ledger;
class ProgramRunner;
class Peers;

namespace bindings
{

class Transaction : public cow::Module
{
public:
    Transaction(cow::MemoryManager &mem,
                const OpContext &op_context,
                credb::trusted::ProgramRunner &runner,
                credb::trusted::Ledger &ledger,
                credb::trusted::Peers &peers,
                LockHandle &lock_handle);

    cow::ValuePtr get_member(const std::string &name) override;

private:
    LockHandle &m_lock_handle;
    credb::trusted::Transaction m_transaction;

    const OpContext &m_op_context;
    credb::trusted::ProgramRunner &m_runner;
    credb::trusted::Ledger &m_ledger;
    credb::trusted::Peers &m_peers;
};

}
}
}
