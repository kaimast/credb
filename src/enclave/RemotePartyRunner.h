/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "ProgramRunner.h"

namespace credb::trusted
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
                      const std::string &collection,
                      const std::string &key,
                      identity_uid_t transaction_root = INVALID_UID,
                      transaction_id_t transaction_id = INVALID_TRANSACTION_ID);

    virtual ~RemotePartyRunner() = default;

private:
    void handle_done() override;

    std::shared_ptr<RemoteParty> m_remote_party;
    taskid_t m_task_id;
    operation_id_t m_op_id;
};

} // namespace credb::trusted
