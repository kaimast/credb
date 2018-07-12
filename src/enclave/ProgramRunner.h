#pragma once

#include "LockHandle.h"
#include "OpContext.h"
#include "Task.h"
#include "Transaction.h"

#include <cowlang/Interpreter.h>
#include <thread>

namespace credb
{
namespace trusted
{

class RemoteParty;

class ProgramRunner : public Task
{
public:
    /**
     * Constructor 
     *
     * By default it will be created without the context of a transaction
     * If a remote transaction called this program, transaction_root and transaction_id must be set accordingly
     */
    ProgramRunner(Enclave &enclave,
                  bitstream &&data,
                  const std::string &collection,
                  const std::string &program_name,
                  const std::vector<std::string> &args,
                  identity_uid_t transaction_root = INVALID_UID,
                  transaction_id_t transaction_id = INVALID_TRANSACTION_ID);

    virtual ~ProgramRunner();

    void handle_op_response() override;

    cow::ValuePtr get_result() const;

    const std::string &program_name() const { return m_op_context.program_name(); }

    void run();
    
    bool has_errors() const { return m_errors.size() > 0; }

    const std::string &get_errors() const { return m_errors; }

    const OpContext& op_context() const
    {
        return m_op_context;
    }

protected:
    void work() override;

private:
    std::string m_errors;

    const OpContext m_op_context;

    LockHandle m_lock_handle;

    bitstream m_data;

    cow::Interpreter m_interpreter;
    cow::ValuePtr m_result = nullptr;
};

typedef std::shared_ptr<ProgramRunner> ProgramRunnerPtr;

inline cow::ValuePtr ProgramRunner::get_result() const { return m_result; }

} // namespace trusted
} // namespace credb
