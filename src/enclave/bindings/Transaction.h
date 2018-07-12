#pragma once

#include <cowlang/Interpreter.h>

#include "../Transaction.h"
#include "Database.h"

namespace credb
{
namespace trusted
{

class OpContext;
class LockHandle;

namespace bindings
{

class Transaction : public credb::trusted::bindings::Database
{
public:
    /**
     * Constructor for a local transaction
     */
    Transaction(cow::MemoryManager &mem,
                credb::trusted::Ledger &ledger,
                credb::trusted::Enclave &enclave,
                credb::trusted::ProgramRunner &runner,
                LockHandle &lock_handle_);

    /**
     * Constructor for a remote transaction
     */
    Transaction(cow::MemoryManager &mem,
                credb::trusted::Ledger &ledger,
                credb::trusted::Enclave &enclave,
                credb::trusted::ProgramRunner &runner,
                credb::trusted::TransactionPtr transaction,
                credb::trusted::LockHandle &lock_handle);

    ~Transaction();

    bool is_transaction() const override
    {
        return true;
    }

    virtual cow::ValuePtr get_member(const std::string &name) override;

private:
    void abort();

    credb::trusted::TransactionPtr m_transaction;
};

}
}
}
