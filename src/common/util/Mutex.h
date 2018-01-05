#pragma once
#include <mutex>

#ifdef DEBUG_MUTEX
// warning: this may cause running out of memory
#include <atomic>
#include <cstdlib>
#include <string>
#include <unordered_set>
#ifdef IS_ENCLAVE
#include "../../enclave/logging.h"
#define _mutex_log(x) log_debug(x)
#else
#include <glog/logging.h>
#define _mutex_log(x) DLOG(INFO) << x
#endif

namespace credb
{
class Mutex
{
    std::mutex m_mutex;
    const int m_id;
    static std::atomic<int> m_static_next_id;
    static __thread std::unordered_set<int> *m_thread_holding;
    // FIXME: thread_local in SGX SDK 1.9 gives linker error
    // In function `__tls_init': undefined reference to `__cxa_thread_atexit'

    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    static std::unordered_set<int> *get_holding_set()
    {
        if(!m_thread_holding)
            m_thread_holding = new std::unordered_set<int>();
        // don't care about the memory leak because only in DEBUG mode
        return m_thread_holding;
    }

    static bool is_holding(int id)
    {
        auto s = get_holding_set();
        return s->count(id);
    }

    static void mark_holding(int id)
    {
        auto s = get_holding_set();
        s->insert(id);
    }

    static void unmark_holding(int id)
    {
        auto s = get_holding_set();
        s->erase(id);
    }

public:
    Mutex() : m_id(++m_static_next_id) {}

    ~Mutex() { unmark_holding(m_id); }

    void lock()
    {
        // _mutex_log("credb::Mutex: Mutex #" + std::to_string(m_id) + " lock()");
        if(is_holding(m_id))
        {
            _mutex_log("credb::Mutex: Mutex #" + std::to_string(m_id) + " already locked!");
            abort();
        }
        m_mutex.lock();
        mark_holding(m_id);
    }

    void unlock()
    {
        // _mutex_log("credb::Mutex: Mutex #" + std::to_string(m_id) + " unlock()");
        if(!is_holding(m_id))
        {
            _mutex_log("credb::Mutex: Mutex #" + std::to_string(m_id) + " is not locked!");
            abort();
        }
        m_mutex.unlock();
        unmark_holding(m_id);
    }

    bool try_lock()
    {
        if(is_holding(m_id))
        {
            _mutex_log("credb::Mutex: Mutex #" + std::to_string(m_id) + " try_lock() but already locked!");
            abort();
        }
        bool res = m_mutex.try_lock();
        // _mutex_log("credb::Mutex: Mutex #" + std::to_string(m_id) + " try_lock() " + (res ?
        // "success" : "failure"));
        if(res)
            mark_holding(m_id);
        return res;
    }
};
} // namespace credb

#undef _mutex_log
#else
namespace credb
{
typedef std::mutex Mutex;
}
#endif

namespace credb
{
typedef std::lock_guard<Mutex> LockGuard;
typedef std::unique_lock<Mutex> UniqueLock;
} // namespace credb
