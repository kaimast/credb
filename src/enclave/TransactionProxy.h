#pragma once

#include "TransactionExecutor.h"
#include "Task.h"
#include "MiniThread.h"

namespace credb
{
namespace trusted
{

/**
 * Executes transactions on behalf of a client
 */
class TransactionProxy : public Task, public TransactionExecutor
{
public:
    TransactionProxy(bitstream &request, const OpContext &op_context, RemoteParties &remote_parties, TransactionManager &transaction_manager, Enclave &enclave);

    ~TransactionProxy();

    bitstream process();

    void handle_op_response() override;
    
protected:
    void work() override;

private:
    operation_info_t* new_operation_info_from_req(bitstream &req);

    bool m_generate_witness;

    bitstream m_result;
};

}
}
