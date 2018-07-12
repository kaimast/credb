#pragma once

#include <unordered_map>
#include "Transaction.h"
#include "util/pair_hash.h"
#include "Counter.h"
#include "util/Identity.h"

namespace credb
{
namespace trusted
{

class Enclave;

class TransactionManager
{
public:
    TransactionManager(Enclave &enclave)
        : m_enclave(enclave)
    {}

    ~TransactionManager();

    TransactionPtr get(const identity_uid_t node_id, const transaction_id_t tx_id);

    TransactionPtr init_remote_transaction(identity_uid_t transaction_root, transaction_id_t transaction_id, IsolationLevel isolation_level);

    TransactionPtr init_local_transaction(IsolationLevel isolation_level);

    /// This is automatically called once you commit or abort a transaction
    void remove_transaction(Transaction &tx);

    size_t num_pending_transactions() const
    {
        return m_transactions.size();
    }

private:
    std::mutex m_mutex;

    Enclave &m_enclave;

    Counter<uint32_t> m_id_counter;

    std::unordered_map<std::pair<identity_uid_t, transaction_id_t>, TransactionPtr, pair_hash> m_transactions;

};

}
}
