#include "ProgramRunner.h"
#include "machineprimitives.h"

#include "bindings/Database.h"
#include "bindings/Object.h"
#include "bindings/OpContext.h"

#include "Enclave.h"
#include "TaskManager.h"
#include "logging.h"

namespace credb
{
namespace trusted
{

ProgramRunner::ProgramRunner(Enclave &enclave,
                             bitstream &&data,
                             const std::string &collection,
                             const std::string &program_name,
                             const std::vector<std::string> &args)
: Task(enclave), m_op_context(enclave.identity(), collection + "/" + program_name),
  m_lock_handle(enclave.ledger()), m_data(std::move(data)), m_interpreter(m_data)
{
    auto &ledger = enclave.ledger();

    auto db_hook = cow::make_value<bindings::Database>(m_interpreter.memory_manager(), m_op_context, ledger,
                                                       enclave, this, m_lock_handle);
    auto op_ctx_hook = cow::make_value<bindings::OpContext>(m_interpreter.memory_manager(), m_op_context);

    m_interpreter.set_list("argv", args);
    m_interpreter.set_module("op_context", op_ctx_hook);
    m_interpreter.set_module("db", db_hook);

    if(!program_name.empty())
    {
        auto object_hook =
        cow::make_value<bindings::Object>(m_interpreter.memory_manager(), m_op_context, ledger,
                                          collection, program_name, m_lock_handle);
        m_interpreter.set_module("self", object_hook);
    }

    auto arg = reinterpret_cast<arg_t>(this);

    minithread_allocate_stack(&m_thread.stack_base, &m_thread.stack_top);
    minithread_initialize_stack(&m_thread.stack_top, &ProgramRunner::run_program, arg,
                                &ProgramRunner::cleanup_program, arg);
}

ProgramRunner::~ProgramRunner() { minithread_free_stack(m_thread.stack_base); }

void ProgramRunner::run_program(arg_t arg)
{
    auto runner = reinterpret_cast<ProgramRunner*>(arg);
    runner->work();
}

void ProgramRunner::cleanup_program(arg_t arg)
{
    auto runner = reinterpret_cast<ProgramRunner*>(arg);
    minithread_switch(&runner->m_thread.stack_top, &runner->m_host_thread.stack_top);
}

void ProgramRunner::handle_op_response() { run(); }

void ProgramRunner::suspend() { minithread_switch(&m_thread.stack_top, &m_host_thread.stack_top); }

void ProgramRunner::run()
{
    lock();
    // Switch into minithread and execute as much as possible...
    minithread_switch(&m_host_thread.stack_top, &m_thread.stack_top);

    if(is_done())
    {
        cleanup();
    }
    else
    {
        unlock();
    }
}

void ProgramRunner::work()
{
    try
    {
        m_result = m_interpreter.execute();
    }
    catch(std::exception &e)
    {
        // log_error(std::string("Program execution failed: ") + e.what());

        if(has_errors())
        {
            m_errors = m_errors + "\n" + e.what();
        }
        else
        {
            m_errors = e.what();
        }
    }

    m_lock_handle.clear();
    mark_done();
}

} // namespace trusted
} // namespace credb
