/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include <list>
#include <unordered_map>

#include "Counter.h"
#include "MiniThread.h"
#include "Task.h"
#include <mutex>

namespace credb
{
namespace trusted
{

class TaskManager
{
public:
    taskid_t next_identifier() { return m_tid_counter.next(); }

    void register_task(std::shared_ptr<Task> task);
    void unregister_task(taskid_t identifier);

    TaskPtr get_task(taskid_t identifier)
    {
        std::lock_guard<std::mutex> task_lock(m_task_mutex);
        auto it = m_tasks.find(identifier);

        if(it == m_tasks.end())
        {
            throw std::runtime_error("No such task");
        }

        return it->second;
    }

private:
    Counter<taskid_t> m_tid_counter;
    std::unordered_map<taskid_t, std::shared_ptr<Task>> m_tasks;

    std::mutex m_task_mutex;
};

} // namespace trusted
} // namespace credb
