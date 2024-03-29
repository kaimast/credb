/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "TaskManager.h"

#include "logging.h"

namespace credb::trusted
{

void TaskManager::register_task(std::shared_ptr<Task> task)
{
    std::lock_guard<std::mutex> task_lock(m_task_mutex);
    m_tasks.emplace(task->identifier(), task);
}

void TaskManager::unregister_task(taskid_t identifier)
{
    std::lock_guard<std::mutex> task_lock(m_task_mutex);
    auto it = m_tasks.find(identifier);

    if(it == m_tasks.end())
    {
        log_error("No such task: " + std::to_string(identifier));
    }
    else
    {
        m_tasks.erase(it);
    }
}

} // namespace credb::trusted
