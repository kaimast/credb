/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "TransactionHandle.h"

namespace credb::trusted
{

transaction_bounds_t TransactionHandle::get_boundaries() const
{
    //TODO do remote ops too
    auto ops = local_ops();
    transaction_bounds_t result;

    auto parse_op = [&result] (const event_id_t &id) -> void
    {
        auto it = result.find(id.shard);

        if(it == result.end())
        {
            event_range_t range = {id.block, id.block, id.index, id.index};
            result.emplace(id.shard, range);
        }
        else
        {
            auto &e = it->second;

            if(id < e)
            {
                e.start_block = id.block;
                e.start_index = id.index;
            }
            else if(id > e)
            {
                e.end_block = id.block;
                e.end_index = id.index;
            }
        }
    };

    for(const auto &op: ops.reads)
    {   
        parse_op(op);
    }

    for(const auto &op: ops.writes)
    {
        parse_op(op);
    }

    return result;
}

std::map<identity_uid_t, op_set_t> TransactionHandle::remote_ops() const
{
    std::map<identity_uid_t, op_set_t> result;

    json::Document remote_ops(m_document, 4);

    if(remote_ops.get_size() % 3 != 0)
    {
        throw std::runtime_error("Invalid data format!");
    }

    for(size_t i = 0; i < remote_ops.get_size(); i += 3)
    {
        identity_uid_t idty = json::Document(remote_ops, i).as_integer();

        json::Document read_set(remote_ops, i+1);
        json::Document write_set(remote_ops, i+2);

        op_set_t op_set = {.reads = read_ops(read_set),
        .writes = read_ops(write_set)};
        
        result.emplace(idty, std::move(op_set));
    }

    return result;
}

} // namespace credb::trusted
