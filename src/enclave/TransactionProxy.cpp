/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "TransactionProxy.h"
#include "Transaction.h"
#include "TransactionManager.h"
#include "logging.h"
#include "op_info.h"

namespace credb::trusted
{

TransactionProxy::TransactionProxy(bitstream &request, const OpContext &op_context, RemoteParties &remote_parties, TransactionManager &transaction_manager, Enclave &enclave)
    : Task(enclave), TransactionExecutor(nullptr, remote_parties, this)
{
    IsolationLevel isolation;
    
    request >> reinterpret_cast<uint8_t&>(isolation) >> m_generate_witness;

    auto transaction = transaction_manager.init_local_transaction(isolation);

    set_transaction(transaction);

    transaction->init_task(identifier(), op_context);

    // construct op history and also collect shards_lock_type
    uint32_t ops_size = 0;
    request >> ops_size;

    for(uint32_t i = 0; i < ops_size; ++i)
    {
        auto op = new_operation_info_from_req(request);
        if(!op)
        {
            return;
        }

        transaction->register_operation(op);
    }

}

TransactionProxy::~TransactionProxy() = default;

operation_info_t* TransactionProxy::new_operation_info_from_req(bitstream &req)
{
    OperationType op;
    req >> reinterpret_cast<uint8_t &>(op);

    auto &tx = get_transaction();
    
    switch(op)
    {
    case OperationType::GetObject:
        return new get_info_t(tx, req, identifier());
    case OperationType::HasObject:
        return new has_obj_info_t(tx, req, identifier());
    case OperationType::CheckObject:
        return new check_obj_info_t(tx, req, identifier());
    case OperationType::PutObject:
        return new put_info_t(tx, req, identifier());
    case OperationType::AddToObject:
        return new add_info_t(tx, req, identifier());
    case OperationType::FindObjects:
        return new find_info_t(tx, req, identifier());
    case OperationType::RemoveObject:
        return new remove_info_t(tx, req, identifier());
    default:
        get_transaction().set_error("Unknown OperationType " + std::to_string(static_cast<uint8_t>(op)));
        log_error(get_transaction().error());
        return nullptr;
    }
}

bitstream TransactionProxy::process()
{
    auto &tx = get_transaction();

    if(tx.is_distributed())
    {
        // If the transaction is distributed we have to initialize a userspace thread
        Task::setup_thread();
        Task::switch_into_thread();
    }
    else
    {
        work();
    }

    return std::move(m_result);
}

void TransactionProxy::work()
{
    if(!phase_one(m_generate_witness))
    {
        m_result << false << get_transaction().error();
    }
    else
    {
        auto witness = phase_two(m_generate_witness);
        m_result << true << witness;
    }
}

void TransactionProxy::handle_op_response()
{
    Task::switch_into_thread();
}

} // namespace credb::trusted
