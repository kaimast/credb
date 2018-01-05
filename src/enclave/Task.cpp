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

void Task::cleanup() { m_task_manager.unregister_task(identifier()); }

} // namespace trusted
} // namespace credb
