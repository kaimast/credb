#pragma once

#include "ProgramRunner.h"

namespace credb
{
namespace trusted
{

class RemotePartyRunner : public ProgramRunner
{
public:
    RemotePartyRunner(Enclave &enclave,
                      std::shared_ptr<RemoteParty> remote_party,
                      taskid_t task_id,
                      operation_id_t op_id,
                      bitstream &&data,
                      const std::vector<std::string> &args,
                      const OpContext &op_context,
                      const std::string &collection,
                      const std::string &key);

private:
    void handle_done() override;

    std::shared_ptr<RemoteParty> m_remote_party;
    taskid_t m_task_id;
    operation_id_t m_op_id;
};

} // namespace trusted
} // namespace credb
