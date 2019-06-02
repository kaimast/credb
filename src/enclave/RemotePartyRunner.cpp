/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "RemotePartyRunner.h"
#include "RemoteParty.h"
#include "logging.h"
#include "util/FunctionCallResult.h"

namespace credb::trusted
{

RemotePartyRunner::RemotePartyRunner(Enclave &enclave,
                                     std::shared_ptr<RemoteParty> remote_party,
                                     taskid_t task_id,
                                     operation_id_t op_id,
                                     bitstream &&data,
                                     const std::vector<std::string> &args,
                                     const std::string &collection,
                                     const std::string &key,
                                     identity_uid_t transaction_root,
                                     transaction_id_t transaction_id)
: ProgramRunner(enclave, std::move(data), collection, key, args, transaction_root, transaction_id),
  m_remote_party(remote_party), m_task_id(task_id), m_op_id(op_id)
{
}

void RemotePartyRunner::handle_done()
{
    bitstream output;

    // FIXME task id?
    output << static_cast<mtype_data_t>(MessageType::OperationResponse);
    output << m_task_id << m_op_id;

    try
    {
        if(has_errors())
        {
            output << FunctionCallResult::ProgramFailure << get_errors();
        }
        else
        {
            auto result = get_result();
            output << FunctionCallResult::Success << result;
        }
    }
    catch(std::exception &e)
    {
        std::string errorstr = std::string("failed to convert result value: ") + e.what();
        log_warning(errorstr);
        output << FunctionCallResult::ProgramFailure << errorstr;
    }

    m_remote_party->lock();
    m_remote_party->send(output);
    m_remote_party->unlock();
}

} // namespace credb::trusted
