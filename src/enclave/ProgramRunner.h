#pragma once

#include "LockHandle.h"
#include "MiniThread.h"
#include "OpContext.h"
#include "Task.h"

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
    ProgramRunner(Enclave &enclave,
                  bitstream &&data,
                  const std::string &collection,
                  const std::string &program_name,
                  const std::vector<std::string> &args);

    virtual ~ProgramRunner();

    void handle_op_response() override;

    cow::ValuePtr get_result() const;

    const std::string &program_name() const { return m_op_context.program_name(); }

    void run();

    void suspend();

    bool has_errors() const { return m_errors.size() > 0; }

    const std::string &get_errors() const { return m_errors; }

private:
    void work();

    static void run_program(arg_t arg);
    static void cleanup_program(arg_t arg);

    std::string m_errors;

    const OpContext m_op_context;

    LockHandle m_lock_handle;

    bitstream m_data;

    MiniThread m_host_thread;
    MiniThread m_thread;

    cow::Interpreter m_interpreter;
    cow::ValuePtr m_result = nullptr;
};

typedef std::shared_ptr<ProgramRunner> ProgramRunnerPtr;

inline cow::ValuePtr ProgramRunner::get_result() const { return m_result; }

} // namespace trusted
} // namespace credb
