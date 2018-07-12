#pragma once

#include "Transaction.h"

namespace credb
{
namespace trusted
{
namespace bindings
{

/**
 * Same as transaction but commit() doesn't do anything
 */
class NestedTransaction : public credb::trusted::bindings::Transaction
{
public:
    NestedTransaction(cow::MemoryManager &mem, Transaction &parent,
                    credb::trusted::Ledger &ledger,
                    credb::trusted::Enclave &enclave,
                    credb::trusted::ProgramRunner &runner,
                    credb::trusted::TransactionPtr transaction,
                    credb::trusted::LockHandle &lock_handle);

    cow::ValuePtr get_member(const std::string &name) override;

private:
    Transaction &m_parent;
};

}
}
}
