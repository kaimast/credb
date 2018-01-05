#include "RWLockable.h"

bool RWLockable::try_read_lock()
{
    if(!m_mutex.try_lock())
    {
        return false;
    }

    m_read_count += 1;
    m_mutex.unlock();

    return true;
}

void RWLockable::read_to_write_lock()
{
    m_mutex.lock();

    if(m_read_count == 0)
    {
        throw std::runtime_error("Call read_lock() first");
    }

    m_read_count -= 1;

    while(m_read_count > 0)
    {
        m_cond.wait(m_mutex);
    }
}

void RWLockable::read_lock()
{
    m_mutex.lock();
    m_read_count += 1;
    m_mutex.unlock();
}

void RWLockable::read_unlock()
{
    m_mutex.lock();

    if(m_read_count == 0)
    {
        throw std::runtime_error("Cannot decrease read count!");
    }
    m_read_count -= 1;

    if(m_read_count == 0)
    {
        m_cond.notify_all();
    }
    m_mutex.unlock();
}

bool RWLockable::try_read_to_write_lock()
{
    if(!m_mutex.try_lock())
    {
        return false;
    }

    if(m_read_count == 0)
    {
        throw std::runtime_error("Call read_lock() first");
    }

    if(m_read_count == 1)
    {
        m_read_count = 0;
        return true;
    }

    m_mutex.unlock();
    return false;
}

void RWLockable::write_to_read_lock()
{
    if(m_read_count > 0)
    {
        throw std::runtime_error("You (probably) don't hold the write lock!");
    }

    m_read_count = 1;

    m_mutex.unlock();
}

bool RWLockable::try_write_lock()
{
    if(!m_mutex.try_lock())
    {
        return false;
    }

    if(m_read_count == 0)
    {
        return true;
    }

    m_mutex.unlock();
    return false;
}

void RWLockable::write_lock()
{
    m_mutex.lock();

    while(m_read_count > 0)
    {
        m_cond.wait(m_mutex);
    }
}

void RWLockable::write_unlock() { m_mutex.unlock(); }

void RWLockable::lock(LockType lock_type)
{
    if(lock_type == LockType::Read)
    {
        read_lock();
    }
    else
    {
        write_lock();
    }
}

void RWLockable::unlock(LockType lock_type)
{
    if(lock_type == LockType::Read)
    {
        read_unlock();
    }
    else
    {
        write_unlock();
    }
}
