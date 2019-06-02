/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#pragma once

#include "MiniThread.h"
#include "util/OperationType.h"
#include "util/Mutex.h"
#include <bitstream.h>
#include <memory>

namespace credb::trusted
{

class TaskManager;
class Enclave;

//! Interface for an asynchronous task
class Task : public std::enable_shared_from_this<Task>, public credb::Mutex
{
public:
    Task(Enclave &enclave);
    virtual ~Task();

    Task(const Task &other) = delete;

    bool is_done() const;

    taskid_t identifier() const { return m_identifier; }

    virtual void handle_op_response() = 0;
    virtual void handle_done() {}

    virtual void work() = 0;

    void suspend();

protected:
    /**
     * Creates thread datastructures
     */
    void setup_thread();

    /**
     * Runs or switches into thread
     */
    void switch_into_thread();

    void mark_done();
    void cleanup();
    Enclave &m_enclave;
    TaskManager &m_task_manager;

private:
    static void run_minithread(arg_t arg);
    static void cleanup_minithread(arg_t arg);

    MiniThread m_host_thread;
    MiniThread m_thread;

    const taskid_t m_identifier;
    bool m_done = false;
};

inline bool Task::is_done() const { return m_done; }

using TaskPtr = std::shared_ptr<Task>;

} // namespace credb::trusted
