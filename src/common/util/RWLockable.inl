//#ifdef FAKE_ENCLAVE
#if 0

inline bool RWLockable::try_read_lock()
{
    return m_mutex.try_lock_shared();
}

inline void RWLockable::read_to_write_lock()
{
    m_mutex.unlock_shared();
    m_mutex.lock();
}

inline bool RWLockable::try_read_to_write_lock()
{
    m_mutex.unlock_shared();
    return m_mutex.try_lock();
}

inline void RWLockable::read_lock()
{
    m_mutex.lock_shared();
}

inline void RWLockable::read_unlock()
{
    m_mutex.unlock_shared();
}

inline void RWLockable::write_to_read_lock()
{
    m_mutex.unlock();
    m_mutex.lock_shared();
}

inline bool RWLockable::try_write_lock()
{
    return m_mutex.try_lock();
}

inline void RWLockable::write_lock()
{
    m_mutex.lock();
}

inline void RWLockable::write_unlock()
{
    m_mutex.unlock();
}

inline void RWLockable::lock(LockType lock_type)
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

inline void RWLockable::unlock(LockType lock_type)
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

#else
inline bool RWLockable::try_read_lock()
{
    while(true)
    {
        auto val = m_read_count.load();

        if(val < 0)
        {
            return false;
        }

        if(m_read_count.compare_exchange_weak(val, val+1))
        {
            return true;
        }
    }
}

inline bool RWLockable::try_read_to_write_lock()
{
    read_unlock();
    return try_write_lock();
}

inline void RWLockable::read_to_write_lock()
{
    while(true)
    {
        auto val = m_read_count.load();

        if(val < 0)
        {
            throw std::runtime_error("read_to_write_lock: invalid state");
        }

        if(val > 1)
        {
            // spin....
            continue;
        }

        if(m_read_count.compare_exchange_weak(val, -1))
        {
            return;
        }
    }
}

inline void RWLockable::read_lock()
{
    while(true)
    {
        auto val = m_read_count.load();

        if(val < 0)
        {
            // spin
            continue;
        }
        else
        {
            if(m_read_count.compare_exchange_weak(val, val+1))
            {
                break;
            }
        }
    }
}

inline void RWLockable::read_unlock()
{
    m_read_count--;
}

inline void RWLockable::write_to_read_lock()
{
    while(true)
    {
        auto val = m_read_count.load();

        if(val != -1)
        {
            throw std::runtime_error("write_to_read_lock: invalid state!");
        }

        if(m_read_count.compare_exchange_weak(val, 1))
        {
            return;
        }
    }
}

inline bool RWLockable::try_write_lock()
{
    while(true)
    {
        auto val = m_read_count.load();

        if(val < 0 || val > 0)
        {
            return false;
        }

        if(m_read_count.compare_exchange_weak(val, -1))
        {
            return true;
        }
    }
}

inline void RWLockable::write_lock()
{
    while(true)
    {
        auto val = m_read_count.load();

        if(val < 0 || val > 0)
        {
            // spin...
            continue;
        }

        if(m_read_count.compare_exchange_weak(val, -1))
        {
            return;
        }
    }
}

inline void RWLockable::write_unlock()
{
    m_read_count.store(0);
}

inline void RWLockable::lock(LockType lock_type)
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

inline void RWLockable::unlock(LockType lock_type)
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


#endif
