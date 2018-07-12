#include "ProgramRunner.h"
#include "machineprimitives.h"

#include "bindings/Database.h"
#include "bindings/Transaction.h"
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
                             const std::vector<std::string> &args,
                             identity_uid_t transaction_root,
                             transaction_id_t transaction_id)
: Task(enclave), m_op_context(enclave.identity(), collection + "/" + program_name),
  m_lock_handle(enclave.ledger()), m_data(std::move(data)), m_interpreter(m_data, true)
{
    auto &ledger = enclave.ledger();
    const bool is_transaction = (transaction_id != INVALID_TRANSACTION_ID);

    if(is_transaction)
    {
#ifndef IS_TEST
        auto &tx_mgr = enclave.transaction_manager();
        auto transaction = tx_mgr.init_remote_transaction(transaction_root, transaction_id, IsolationLevel::Serializable);

        auto tx_hook = cow::make_value<bindings::Transaction>(m_interpreter.memory_manager(), ledger, enclave, *this, transaction, m_lock_handle);

        m_interpreter.set_module("db", tx_hook);
#else
        (void)transaction_root;
        (void)transaction_id;
        throw std::runtime_error("No remote transaction_support in test");
#endif
    }
    else
    {
        auto db_hook = cow::make_value<bindings::Database>(m_interpreter.memory_manager(), m_op_context, ledger, enclave, this, m_lock_handle);
        m_interpreter.set_module("db", db_hook);
    }

    auto op_ctx_hook = cow::make_value<bindings::OpContext>(m_interpreter.memory_manager(), m_op_context);

    m_interpreter.set_list("argv", args);
    m_interpreter.set_module("op_context", op_ctx_hook);

    if(!program_name.empty())
    {
        auto object_hook =
        cow::make_value<bindings::Object>(m_interpreter.memory_manager(), m_op_context, ledger, collection, program_name, m_lock_handle);
        m_interpreter.set_module("self", object_hook);
    }

    Task::setup_thread();
}

ProgramRunner::~ProgramRunner() = default;

void ProgramRunner::handle_op_response()
{
    if(is_done())
    {
        log_warning("Can't handle op response. Task terminated");
        return;
    }

    run();
}

void ProgramRunner::run()
{
    lock();
    Task::switch_into_thread();

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
    catch(std::system_error &e)
    {
        // unexpected error
        log_fatal( e.what() );
    }
    catch(std::exception &e)
    {
        if(has_errors())
        {
            m_errors = m_errors + "\n" + e.what();
        }
        else
        {
            m_errors = e.what();
        }

        log_debug("Program errors: " + m_errors);
    }

    m_lock_handle.clear();
    mark_done();
}

} // namespace trusted
} // namespace credb
