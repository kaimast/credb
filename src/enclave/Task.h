#pragma once

#include "util/OperationType.h"
#include "util/Mutex.h"
#include <bitstream.h>
#include <memory>

namespace credb
{
namespace trusted
{

class TaskManager;
class Enclave;

//! Interface for an asynchronous task
class Task : public std::enable_shared_from_this<Task>, public credb::Mutex
{
public:
    Task(Enclave &enclave);

    virtual ~Task() {}

    bool is_done() const;

    taskid_t identifier() const { return m_identifier; }

    virtual void handle_op_response() = 0;
    virtual void handle_done() {}

protected:
    void mark_done();
    void cleanup();
    Enclave &m_enclave;
    TaskManager &m_task_manager;

private:
    const taskid_t m_identifier;
    bool m_done = false;
};

inline bool Task::is_done() const { return m_done; }

} // namespace trusted
} // namespace credb
