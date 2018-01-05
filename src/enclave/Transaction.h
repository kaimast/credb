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

class Transaction
{
public:
    Ledger &ledger;
    const OpContext &op_context;
    bitstream &req;
    IsolationLevel isolation;
    bool generate_witness;
    LockHandle lock_handle;
    std::map<shard_id_t, LockType> shards_lock_type; // need to be ordered
    std::string error;
    json::Writer writer;

    Transaction(Ledger &ledger_, const OpContext &op_context_, bitstream &req_)
    : ledger(ledger_), op_context(op_context_), req(req_), lock_handle(ledger)
    {
    }
    
    void commit();
    
    void get_output(bitstream &output);

    void set_read_lock_if_not_present(shard_id_t sid);

    bool check_repeatable_read(ObjectEventHandle &obj,
                           const std::string &collection,
                           const std::string &key,
                           shard_id_t sid,
                           const event_id_t &eid);

    ~Transaction();

private:
    std::vector<operation_info_t *> ops;
    Witness witness;


};

}
}
