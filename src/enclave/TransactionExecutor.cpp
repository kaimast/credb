#include "TransactionExecutor.h"
#include "TransactionManager.h"
#include "RemoteParties.h"
#include "PendingWitnessResponse.h"
#include "Peer.h"
#include "Task.h"
#include "logging.h"
#include "wait.h"

namespace credb
{
namespace trusted
{

bool TransactionExecutor::phase_one(bool generate_witness)
{
    auto &tx = *m_transaction;
    bool success = true;

    if(tx.is_remote())
    {
        throw std::runtime_error("Cannot commit remote transaction");
    }

    std::vector<PendingBooleanResponse> responses;

    if(m_task == nullptr && tx.is_distributed())
    {
        throw std::runtime_error("Cannot executed distributedly. No associated task");
    }

    if(tx.is_distributed())
    {
        // might cause deadlocks
        tx.lock_handle().set_blocking(false);
    }

    for(auto &child: tx.children())
    {
        auto peer = m_remote_parties.find_by_uid<Peer>(child);

        if(peer)
        {
            auto op_id = peer->get_next_operation_id();
            auto req = RemoteParty::generate_op_request(m_task->identifier(), op_id, OperationType::TransactionPrepare);

            req << tx.get_root() << tx.identifier() << generate_witness;

            peer->lock();

            peer->send(req);
            PendingBooleanResponse pending(op_id, *peer);
            responses.emplace_back(std::move(pending));

            peer->unlock();
        }
        else
        {
            log_warning("cannot prepare: no such peer");
            success = false;
        }
    }

    if(!tx.prepare(generate_witness))
    {
        success = false;
    }

    if(tx.is_distributed())
    {
        if(!wait_for(responses, *m_task))
        {
            success = false;
        }
    }

    // One or more nodes weren't successful so abort everybody
    if(!success)
    {
        abort();
    }

    return success; 
}

void TransactionExecutor::abort()
{
    auto &tx = *m_transaction;

    if(tx.is_remote())
    {
        throw std::runtime_error("Cannot abort remote transaction");
    }

    std::vector<PendingBooleanResponse> responses;

    for(auto &child: tx.children())
    {
        auto peer = m_remote_parties.find_by_uid<Peer>(child);

        if(!peer)
        {
            log_error("can't abort: no such remote party");
        }
        else
        {
            auto op_id = peer->get_next_operation_id();
            auto req = RemoteParty::generate_op_request(m_task->identifier(), op_id, OperationType::TransactionAbort);

            req << tx.get_root() << tx.identifier();

            peer->lock();

            peer->send(req);
            PendingBooleanResponse pending(op_id, *peer);
            responses.emplace_back(std::move(pending));

            peer->unlock();
        }
    }

    tx.abort();

    if(tx.is_distributed())
    {   
        wait_for(responses, *m_task);
    }
}

Witness TransactionExecutor::phase_two(bool generate_witness)
{
    auto &tx = *m_transaction;

    if(tx.is_remote())
    {
        throw std::runtime_error("Cannot commit remote transaction");
    }

    std::vector<PendingWitnessResponse> responses;

    if(m_task == nullptr)
    {
        throw std::runtime_error("Cannot executed distributedly. No associated task");
    }

    for(auto &child: tx.children())
    {
        auto peer = m_remote_parties.find_by_uid<Peer>(child);

        if(!peer)
        {
            log_error("can't commit: No such remote party! " + std::to_string(child));
        }
        else
        {
            auto op_id = peer->get_next_operation_id();
            auto req = RemoteParty::generate_op_request(m_task->identifier(), op_id, OperationType::TransactionCommit);

            req << tx.get_root() << tx.identifier() << generate_witness;

            peer->lock();

            peer->send(req);
            PendingWitnessResponse pending(op_id, *peer);
            responses.emplace_back(std::move(pending));

            peer->unlock();
        }
    }

    // TODO merge witnesses
    auto witness = tx.commit(generate_witness);

    if(!responses.empty())
    {
        auto res = wait_for(responses, *m_task);

        if(!res)
        {
            log_warning("remote commit failed!");
        }
    }

    return witness;
}

}
}
