/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <json/Writer.h>
#include <map>
#include <unordered_set>

#include "credb/IsolationLevel.h"
#include "credb/Witness.h"
#include "LockHandle.h"
#include "util/Identity.h"
#include "Task.h"
#include "logging.h"
#include "OpContext.h"

namespace credb
{
namespace trusted
{

struct operation_info_t;
class Ledger;
class TransactionLedger;
class TransactionManager;

using transaction_id_t = uint32_t;
constexpr transaction_id_t INVALID_TRANSACTION_ID = 0;

enum class TransactionState : uint8_t
{
    Pending,
    Prepared,
    Committed,
    Aborted
};

/**
 * Server-side logic for transaction processing
 */
class Transaction
{
public:
    /**
     * Constructor
     */
    Transaction(IsolationLevel isolation, Ledger &ledger_, TransactionLedger &transaction_ledger, TransactionManager &tx_mgr, identity_uid_t root, transaction_id_t id, bool is_remote);

    ~Transaction();

    Transaction(const Transaction &other) = delete;

    void init_task(taskid_t tid, const OpContext &context);

    IsolationLevel isolation_level() const
    {
        return m_isolation;
    }

    LockHandle& lock_handle()
    {
        return m_lock_handle;
    }

    Ledger &ledger;

    TransactionState get_state() const
    {
        return m_state;
    }

    void set_error(const std::string &error)
    {
        m_error = error;
    }

    /**
     * Did an error occur?
     */
    bool has_error()
    {
        return !m_error.empty();
    }

    const std::string& error() const
    {
        return m_error;
    }

    bool is_done() const
    {
        return m_state == TransactionState::Committed
            || m_state == TransactionState::Aborted;
    }

    /**
     * Was this transaction initiated by a remote party? 
     */
    bool is_remote() const
    {
        return m_is_remote;
    }

    const OpContext& get_op_context(const taskid_t &task) const
    {
        auto it = m_op_contexts.find(task);

        if(it == m_op_contexts.end())
        {
            throw std::runtime_error("No such op context!");
        }

        return it->second;
    }

    /**
     * Get the origin of this transaction
     */
    identity_uid_t get_root() const
    {
        return m_root;
    }

    /**
     * Local identifier of the transaction
     * Together with the root this forms a uid of the transaction
     */
    transaction_id_t identifier() const
    {
        return m_identifier;
    }

    /**
     * @brief Performs main transaction logic (verification of reads + application of writes)
     *
     * Returns a witness on success (witness may be empty if generate witness is set to false
     * Throws an exception if commit fails
     */
    Witness commit(bool generate_witness);

    /**
     * Validate that the transaction's behavior has not changed and acquires all necessary locks
     *
     *  @param generate_witness
     *      If set true, it will write all reads to the witness
     */
    bool prepare(bool generate_witness);

    void get_output(bitstream &output);

    /**
     * Sets a read lock for a shard to read if no lock for it has been set yet
     */ 
    void set_read_lock(shard_id_t sid);

    /**
     * Sets a write lock for a shard
     */
    void set_write_lock(shard_id_t sid);

    bool check_repeatable_read(ObjectEventHandle &obj,
                           const std::string &collection,
                           const std::string &full_path,
                           shard_id_t sid,
                           const event_id_t &expected_eid,
                           taskid_t task);

    /**
     * Add a new operation that is associated with this transaction
     *
     * @param op
     *      The operation structure. Memory will be managed by the transaction object from here on
     */
    void register_operation(operation_info_t *op);
    
    json::Writer writer;

    /**
     * Discard transaction and release all locks
     *
     * If transaction already has committed this will *not* undo the transaction
     */
    void abort();

    void add_child(identity_uid_t identity)
    {
        if(m_state != TransactionState::Pending)
        {
            log_error("Cannot add child: invalid state");
            return;
        }

        m_children.insert(identity);
    }

    /**
     * Get the set of nodes that are part of this transaction
     */
    const std::unordered_set<identity_uid_t>& children() const
    {
        return m_children;
    }

    /**
     * Is this a distributed transaction that spans multiple nodes?
     */
    bool is_distributed() const
    {
        return is_remote() || !m_children.empty();
    }

private:
    void cleanup();

    const IsolationLevel m_isolation;

    std::unordered_map<taskid_t, OpContext> m_op_contexts; 

    TransactionLedger &m_transaction_ledger;

    TransactionManager &m_transaction_mgr;

    LockHandle m_lock_handle;

   /**
     * Keep a list of all locks that need to be acquired
     * @note this needs to be ordered so we don't deadlock
     */
    std::map<shard_id_t, LockType> m_shard_lock_types;

    std::string m_error;

    /**
     * The remote party that initiated the transaction
     *
     */
    identity_uid_t m_root;

    transaction_id_t m_identifier;
    const bool m_is_remote;

    /**
     * If this is the root transaction,
     * this can contain child transactions at other nodes
     *
     * @brief A set of UIDs 
     */
    std::unordered_set<identity_uid_t> m_children;

    std::vector<operation_info_t*> m_ops;

    std::unordered_set<identity_uid_t> m_call_set;

    TransactionState m_state = TransactionState::Pending;
};

using TransactionPtr = std::shared_ptr<Transaction>;

}
}
