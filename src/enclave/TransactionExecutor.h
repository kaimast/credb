/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "Transaction.h"

namespace credb
{
namespace trusted
{

class Task;
class RemoteParties;
class TransactionManager;

class TransactionExecutor
{
public:
    TransactionExecutor(TransactionPtr transaction, RemoteParties &remote_parties, Task *task)
        : m_transaction(transaction), m_remote_parties(remote_parties), m_task(task)
    {}

    TransactionExecutor(const TransactionExecutor &other) = delete;

    void abort();

    bool phase_one(bool generate_witness);
    Witness phase_two(bool generate_witness);

    Transaction& get_transaction()
    {
        return *m_transaction;
    }

protected:
    void set_transaction(TransactionPtr ptr)
    {
        if(m_transaction)
        {
            throw std::runtime_error("Transaction already set!");
        }
        else
        {
            m_transaction = ptr;
        }
    }

private:
    TransactionPtr m_transaction;
    RemoteParties &m_remote_parties;

    /// The associated task that is executing the transaction
    Task *m_task;
};

}
}
