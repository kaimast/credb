#pragma once

#include <json/Writer.h>
#include <map>

#include "credb/IsolationLevel.h"
#include "credb/Witness.h"
#include "LockHandle.h"

namespace credb
{
namespace trusted
{

struct operation_info_t;
class Ledger;
class OpContext;

/**
 * Server-side logic for transaction processing
 */
class Transaction
{
private:
    IsolationLevel m_isolation;

    /**
     * Keep a list of all locks that need to be acquired
     * @note this needs to be ordered so we don't deadlock
     */
    std::map<shard_id_t, LockType> m_shard_lock_types;

public:
    IsolationLevel isolation_level() const
    {
        return m_isolation;
    }

    Ledger &ledger;
    const OpContext &op_context;
    bool generate_witness;
    LockHandle lock_handle;

    std::string error;

    Transaction(IsolationLevel isolation, Ledger &ledger_, const OpContext &op_context_, LockHandle *lock_handle_);
    Transaction(bitstream &request, Ledger &ledger_, const OpContext &op_context_);

    ~Transaction();

    /**
     * @brief Performs main transaction logic (verification of reads + application of writes)
     *
     * Returns a witness on success (witness may be empty if generate witness is set to false
     * Throws an exception if commit fails
     */
    Witness commit();

    bool phase_one();
    Witness phase_two();

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
                           const event_id_t &eid);

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
     */
    void clear();

private:
    operation_info_t* new_operation_info_from_req(bitstream &req);

    std::vector<operation_info_t*> m_ops;
};

}
}
