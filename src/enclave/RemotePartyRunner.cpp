#include "RemotePartyRunner.h"
#include "RemoteParty.h"
#include "logging.h"

namespace credb
{
namespace trusted
{

RemotePartyRunner::RemotePartyRunner(Enclave &enclave,
                                     std::shared_ptr<RemoteParty> remote_party,
                                     taskid_t task_id,
                                     operation_id_t op_id,
                                     bitstream &&data,
                                     const std::vector<std::string> &args,
                                     const OpContext &op_context,
                                     const std::string &collection,
                                     const std::string &key)
: ProgramRunner(enclave, std::move(data), collection, key, args), m_remote_party(remote_party),
  m_task_id(task_id), m_op_id(op_id)
{
    // FIXME use context
    (void)op_context;
}

void RemotePartyRunner::handle_done()
{
    bitstream output;

    // FIXME task id?
    output << static_cast<mtype_data_t>(MessageType::OperationResponse);
    output << m_task_id << m_op_id;

    bitstream payload;

    try
    {
        if(has_errors())
        {
            payload << false << get_errors();
        }
        else
        {
            auto result = get_result();
            payload << true << result;
        }
    }
    catch(std::exception &e)
    {
        std::string errorstr = "failed to convert result value";
        log_warning(errorstr);
        payload << false << errorstr;
    }

    output << payload;

    m_remote_party->lock();
    m_remote_party->send(output);
    m_remote_party->unlock();
}

} // namespace trusted
} // namespace credb
