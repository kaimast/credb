#pragma once

#include "util/Mutex.h"
#include <condition_variable>
#include <stdexcept>

enum class LockType
{
    Read,
    Write
};

/// Basic readers writers lock
/// Allows multiple readers but only one writer
class RWLockable
{
public:
    virtual ~RWLockable() {}

    void read_lock();

    /// Converts a read to a write lock
    /// \note Only call this if you hold a read-lock already!
    void read_to_write_lock();

    /// Converts a write to a read lock
    /// \note Only call this if you already hold the write-lock!
    void write_to_read_lock();

    bool try_read_to_write_lock();

    bool try_write_lock();
    bool try_read_lock();

    void write_lock();

    void read_unlock();
    void write_unlock();

    void lock(LockType lock_type);
    void unlock(LockType lock_type);

private:
    credb::Mutex m_mutex;
    std::condition_variable_any m_cond;

    uint32_t m_read_count = 0;
};
