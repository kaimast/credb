/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "TransactionManager.h"
#include "Enclave.h"

namespace credb
{
namespace trusted
{

TransactionManager::~TransactionManager()
{
    while(true)
    {
        m_mutex.lock();
        
        TransactionPtr tx;
        auto it = m_transactions.begin();

        if(it == m_transactions.end())
        {
            tx = nullptr;
        }
        else
        {
            tx = it->second;
        }
        m_mutex.unlock();

        if(tx == nullptr)
        {
            //done
            break;
        }
        else
        {
            tx->abort();
        }
    }
}

TransactionPtr TransactionManager::init_local_transaction(IsolationLevel isolation_level)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto uid = m_enclave.identity().get_unique_id();
    auto lid = m_id_counter.next();

    auto tx = std::make_shared<Transaction>(isolation_level, m_enclave.ledger(), m_enclave.transaction_ledger(), *this, uid, lid, false);

    m_transactions[std::pair(uid, lid)] = tx;
    return tx;
}

TransactionPtr TransactionManager::init_remote_transaction(identity_uid_t transaction_root, transaction_id_t transaction_id, IsolationLevel isolation_level)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if it was already initialized
    auto it = m_transactions.find({transaction_root, transaction_id});

    if(it == m_transactions.end())
    {
        auto &identity_db = m_enclave.identity_database();

        if(!identity_db.has_identity(transaction_root))
        {
            throw std::runtime_error("Invalid transaction root");
        }

        auto tx = std::make_shared<Transaction>(isolation_level, m_enclave.ledger(), m_enclave.transaction_ledger(), *this, transaction_root, transaction_id, true);

        m_transactions[std::pair(transaction_root, transaction_id)] = tx;
        return tx;
    }
    else
    {
        return it->second;
    }
}

TransactionPtr TransactionManager::get(const identity_uid_t node_id, const transaction_id_t tx_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_transactions.find({node_id, tx_id});

    if(it == m_transactions.end())
    {
        return {nullptr};
    }
    else
    {
        return it->second;
    }
}

void TransactionManager::remove_transaction(Transaction &tx)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto node_id = tx.get_root();
    auto tx_id = tx.identifier();

    auto it = m_transactions.find({node_id, tx_id});

    if(it == m_transactions.end())
    {
        throw std::runtime_error("Cannot remove; no such transaction!");
    }

    m_transactions.erase(it);
}

}
}
