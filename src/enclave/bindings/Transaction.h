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

namespace bindings
{

class Transaction : public cow::Module
{
public:
    Transaction(cow::MemoryManager &mem,
                const OpContext &op_context,
                credb::trusted::Ledger &ledger,
                LockHandle &lock_handle);

    cow::ValuePtr get_member(const std::string &name) override;

private:
    credb::trusted::Transaction m_transaction;

    const OpContext &m_op_context;
    credb::trusted::Ledger &m_ledger;
    LockHandle &m_lock_handle;
};

}
}
}
