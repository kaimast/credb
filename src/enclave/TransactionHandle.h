#pragma once

#include <unordered_map>
#include <vector>
#include <json/Document.h>
#include <unordered_set>

#include "util/Identity.h"
#include "util/event_id_hash.h"
#include "ledger_pos.h"
#include "Transaction.h"

namespace credb
{
namespace trusted
{

struct op_set_t
{
    std::unordered_set<event_id_t> reads;
    std::unordered_set<event_id_t> writes;
};

inline bool operator==(const op_set_t &first, const op_set_t &second)
{
    return first.reads == second.reads && first.writes == second.writes;
}

/**
 * Similar to ObjectHandle
 *
 * Will help you convert binary representation of a transaction record human readable stuff
 */
class TransactionHandle
{
public:
    TransactionHandle(json::Document doc)
        : m_document(std::move(doc))
    {
    }

    TransactionHandle(TransactionHandle &&other)
        : m_document(std::move(other.m_document))
    {}

    std::string op_context(taskid_t task) const
    {
        json::Document view(m_document, 0);
        return json::Document(view, std::to_string(task)).as_string();
    }

    identity_uid_t transaction_root() const
    {
        json::Document view(m_document, 1);
        return view.as_integer();
    }

    transaction_id_t transaction_id() const
    {
        json::Document view(m_document, 2);

        return view.as_integer();
    }

    op_set_t local_ops() const
    {
        json::Document local_ops(m_document, 3);
        json::Document read_set(local_ops, 0);
        json::Document write_set(local_ops, 1);

        op_set_t op_set = {.reads = read_ops(read_set),
            .writes = read_ops(write_set)};

        return op_set;
    }

    transaction_bounds_t get_boundaries() const;
    
    std::unordered_map<identity_uid_t, op_set_t> remote_ops() const;

private:
    inline std::unordered_set<event_id_t> read_ops(const json::Document &doc) const
    {
        std::unordered_set<event_id_t> result;

        if(doc.get_size() % 3 != 0)
        {
            throw std::runtime_error("Invalid data format!");
        }

        for(size_t i = 0; i < doc.get_size(); i += 3)
        {
            shard_id_t shard = json::Document(doc, i).as_integer();
            block_id_t block = json::Document(doc, i+1).as_integer();
            block_index_t index = json::Document(doc, i+2).as_integer();

            result.insert( {shard, block, index} );
        }

        return result;
    }

    json::Document m_document;
};

inline OrderResult order(TransactionHandle &first, TransactionHandle &second)
{
    auto b1 = first.get_boundaries();
    auto b2 = second.get_boundaries();

    return order(b1, b2);
}

}
}
