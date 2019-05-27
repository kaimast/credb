/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information

#pragma once

#include <memory>

#include "credb/Collection.h"
#include "credb/Transaction.h"
#include "credb/Witness.h"

#include "op_info.h"

namespace credb
{

class TransactionCollectionImpl;
class ClientImpl;

class TransactionImpl : public Transaction
{
public:
    TransactionImpl(ClientImpl &client, IsolationLevel isolation);
    ~TransactionImpl();

    TransactionResult commit(bool generate_witness) override;

    bool is_done() const;

    std::shared_ptr<Collection> get_collection(const std::string &name) override;

private:
    friend class TransactionCollectionImpl;

    void queue_op(operation_info_t *info) { m_ops.push_back(info); }
    void assert_not_committed() const;

    ClientImpl &m_client;
    bool m_done;
    std::vector<operation_info_t *> m_ops;
};

inline bool TransactionImpl::is_done() const { return m_done; }

inline void TransactionImpl::assert_not_committed() const
{
    if(m_done)
        throw std::runtime_error("The transaction has finished.");
}


} // namespace credb
