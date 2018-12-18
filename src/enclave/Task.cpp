/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "Task.h"
#include "Enclave.h"
#include "TaskManager.h"

namespace credb
{
namespace trusted
{

Task::Task(Enclave &enclave)
: m_enclave(enclave), m_task_manager(m_enclave.task_manager()),
  m_identifier(m_task_manager.next_identifier())
{
    m_thread = {.stack_base = nullptr, .stack_top = nullptr};
}

Task::~Task()
{
    if(m_thread.stack_base != nullptr)
    {
        minithread_free_stack(m_thread.stack_base);
    }
}

void Task::setup_thread()
{
    if(m_thread.stack_base != nullptr)
    {
        throw std::runtime_error("Thread already set up!");
    }

    auto arg = reinterpret_cast<arg_t>(this);

    minithread_allocate_stack(&m_thread.stack_base, &m_thread.stack_top);
    minithread_initialize_stack(&m_thread.stack_top, &Task::run_minithread, arg, &Task::cleanup_minithread, arg);
}

void Task::mark_done()
{
    if(m_done)
    {
        throw std::runtime_error("Can't mark done twice!");
    }

    m_done = true;
    handle_done();
}

void Task::switch_into_thread()
{
    // Switch into minithread and execute as much as possible...
    minithread_switch(&m_host_thread.stack_top, &m_thread.stack_top);
}

void Task::run_minithread(arg_t arg)
{
    auto task = reinterpret_cast<Task*>(arg);
    task->work();
}

void Task::cleanup_minithread(arg_t arg)
{
    auto task = reinterpret_cast<Task*>(arg);
    minithread_switch(&task->m_thread.stack_top, &task->m_host_thread.stack_top);
}

void Task::suspend() { minithread_switch(&m_thread.stack_top, &m_host_thread.stack_top); }

void Task::cleanup() 
{
    m_task_manager.unregister_task(identifier());
}

} // namespace trusted
} // namespace credb
