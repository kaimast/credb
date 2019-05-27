/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

#include "LockHandle.h"
#include "Ledger.h"
#include "Shard.h"

#include <stdexcept>

namespace credb
{
namespace trusted
{

LockHandle::LockHandle(Ledger &ledger, LockHandle *parent, bool nonblocking)
   : m_ledger(ledger), m_parent(parent), m_nonblocking(nonblocking)
{
}

LockHandle::LockHandle(LockHandle &&other) noexcept
    : m_ledger(other.m_ledger), m_parent(other.m_parent), m_locks(std::move(other.m_locks)), m_nonblocking(other.m_nonblocking)
{
}

LockHandle::~LockHandle()
{
    clear();
}

void LockHandle::clear()
{
    if(m_parent)
    {
        for(auto &it : m_locks)
        {
            auto &info = it.second;

            for(uint32_t i = 0; i < info.read_count; ++i)
            {
                m_parent->release_shard(it.first, LockType::Read);
            }
            for(uint32_t i = 0; i < info.write_count; ++i)
            {
                m_parent->release_shard(it.first, LockType::Write);
            }
        }
    }
    else
    {
        for(auto &it : m_locks)
        {
            auto &lock_info = it.second;
            auto lock_type = lock_info.lock_type();

            if(lock_type == LockType::Write)
            {
                lock_info.shard.write_unlock();
            }
            else
            {
                lock_info.shard.read_unlock();
            }

        }
    }

    m_locks.clear();
}

Shard &LockHandle::get_shard(shard_id_t shard_no, LockType lock_type, bool nonblocking)
{
    nonblocking = nonblocking || m_nonblocking;

    auto it = m_locks.find(shard_no);

    if(it == m_locks.end())
    {
        if(m_parent)
        {
            auto &s = m_parent->get_shard(shard_no, lock_type, nonblocking);
            m_locks.emplace(shard_no, LockInfo(s, lock_type));
            return s;
        }
        else
        {
            auto &s = *m_ledger.m_shards[shard_no];
            bool success = true;

            if(lock_type == LockType::Write)
            {
                if(nonblocking)
                {
                    success = s.try_write_lock();
                }
                else
                {
                    s.write_lock();
                }
            }
            else
            {
                if(nonblocking)
                {
                    success = s.try_read_lock();
                }
                else
                {
                    s.read_lock();
                }
            }

            if(!success)
            {
                throw would_block_exception();
            }

            m_locks.emplace(shard_no, LockInfo(s, lock_type));
            return s;
        }
    }
    else
    {
        if(m_parent)
        {
            auto &lock_info = it->second;

            try {
                auto &s = m_parent->get_shard(shard_no, lock_type, nonblocking);

                if(lock_type == LockType::Write)
                {
                    lock_info.write_count += 1;
                }
                else
                {
                    lock_info.read_count += 1;
                }

                return s;
            }
            catch(would_block_exception &e)
            {
                m_locks.erase(it);
                throw e;
            }
        }
        else
        {
            auto &lock_info = it->second;
            bool success = true;

            if(lock_type == LockType::Write && lock_info.write_count == 0)
            {
                assert(lock_info.read_count > 0);

                if(nonblocking)
                {
                    success = lock_info.shard.try_read_to_write_lock();
                }
                else
                {
                    lock_info.shard.read_to_write_lock();
                }
            }

            if(!success)
            {
                m_locks.erase(it);
                throw would_block_exception();
            }

            if(lock_type == LockType::Write)
            {
                lock_info.write_count += 1;
            }
            else
            {
                lock_info.read_count += 1;
            }

            return lock_info.shard;
        }
    }
}

void LockHandle::release_shard(shard_id_t shard_no, LockType lock_type)
{
    auto it = m_locks.find(shard_no);
    if(it == m_locks.end())
    {
        throw std::runtime_error("No such lock!");
    }

    auto &lock_info = it->second;

    if(m_parent)
    {
        if(lock_type == LockType::Read)
        {
            lock_info.read_count -= 1;
            m_parent->release_shard(shard_no, lock_type);
        }
        else
        {
            lock_info.write_count -= 1;
            m_parent->release_shard(shard_no, lock_type);
        }

        if(lock_info.write_count == 0 && lock_info.read_count == 0)
        {
            m_locks.erase(it);
        }
    }
    else
    {
        if(lock_type == LockType::Read)
        {
            lock_info.read_count -= 1;
            if(lock_info.read_count == 0 && lock_info.write_count == 0)
            {
                lock_info.shard.read_unlock();
                m_locks.erase(it);
            }
        }
        else
        {
            lock_info.write_count -= 1;

            if(lock_info.read_count == 0 && lock_info.write_count == 0)
            {
                lock_info.shard.write_unlock();
                m_locks.erase(it);
            }
            else if(lock_info.write_count == 0)
            {
                lock_info.shard.write_to_read_lock();
            }
        }
    }
}

PageHandle<LedgerBlock> LockHandle::get_block(shard_id_t shard_no, block_id_t block, LockType lock_type)
{
    auto &s = get_shard(shard_no, lock_type);
    return s.get_block(block, lock_type);
}

void LockHandle::release_block(shard_id_t shard_no, block_id_t block, LockType lock_type)
{
    if(block == INVALID_BLOCK)
    {
        return; // no-op
    }
    else
    {
        release_shard(shard_no, lock_type);
    }
}

} // namespace trusted
} // namespace credb
