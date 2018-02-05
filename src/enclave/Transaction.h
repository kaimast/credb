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

public:
    IsolationLevel isolation_level() const
    {
        return m_isolation;
    }

    Ledger &ledger;
    const OpContext &op_context;
    bool generate_witness;
    LockHandle lock_handle;

    /**
     * Keep a list of all locks that need to be aquired
     * @note this needs to be ordered so we don't deadlock
     */
    std::map<shard_id_t, LockType> shards_lock_type;

    std::string error;

    Transaction(IsolationLevel isolation, Ledger &ledger_, const OpContext &op_context_, LockHandle *lock_handle_);
    Transaction(bitstream &request, Ledger &ledger_, const OpContext &op_context_);

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

    void set_read_lock_if_not_present(shard_id_t sid);

    bool check_repeatable_read(ObjectEventHandle &obj,
                           const std::string &collection,
                           const std::string &key,
                           shard_id_t sid,
                           const event_id_t &eid);

    ~Transaction();

    void register_operation(operation_info_t *op);
    
    json::Writer writer;

    void clear();

private:
    operation_info_t* new_operation_info_from_req(bitstream &req);

    std::vector<operation_info_t*> m_ops;
};

}
}
