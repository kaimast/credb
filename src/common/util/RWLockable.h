#pragma once

#include <stdexcept>

//#include <shared_mutex>
#include <atomic>

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

    bool try_write_lock();
    bool try_read_lock();

    /// Will attempt to upgrade the lock
    /// If this fails no lock will be held
    bool try_read_to_write_lock();

    void write_lock();

    void read_unlock();
    void write_unlock();

    void lock(LockType lock_type);
    void unlock(LockType lock_type);

    void wait(LockType lock_type);

private:
#if 0
    // NOTE shared mutex does not work with minithreads
    std::shared_mutex m_mutex;
#else
    std::atomic<int32_t> m_read_count = 0;
#endif
};

class RWHandle
{
public:
    RWHandle(RWLockable &lockable, LockType type)
        : m_lockable(&lockable), m_type(type)
    {
        lock();
    }

    RWHandle()
        : m_lockable(nullptr)
    {}

    ~RWHandle()
    {
        clear();
    }

    void clear()
    {
        if(valid())
        {
            unlock();
        }

        m_lockable = nullptr;
    }

    RWHandle(RWHandle &&other)
        : m_lockable(other.m_lockable), m_type(other.m_type)
    {
        other.m_lockable = nullptr;
    }

    bool valid() const
    {
        return m_lockable != nullptr;
    }

    void operator=(RWHandle &&other)
    {
        m_lockable = other.m_lockable;
        m_type = other.m_type;

        other.m_lockable = nullptr;
    }

    void lock()
    {
        if(m_type == LockType::Read)
        {
            m_lockable->read_lock();
        }
        else
        {
            m_lockable->write_lock();
        }
    }

    void unlock()
    {
        if(m_type == LockType::Read)
        {
            m_lockable->read_unlock();
        }
        else
        {
            m_lockable->write_unlock();
        }
    }

    RWLockable& lockable()
    {
        return *m_lockable;
    }

private:
    RWLockable *m_lockable;
    LockType m_type;
};

class ReadLock : public RWHandle
{
public:
    ReadLock(RWLockable &lockable)
        : RWHandle(lockable, LockType::Read)
    {}
};

class WriteLock : public RWHandle
{
public:
    WriteLock(RWLockable &lockable)
        : RWHandle(lockable, LockType::Write)
    {}
};


#include "RWLockable.inl"
