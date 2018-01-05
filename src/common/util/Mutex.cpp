#include "Mutex.h"

#ifdef DEBUG_MUTEX
namespace credb
{
std::atomic<int> Mutex::m_static_next_id;
__thread std::unordered_set<int> *Mutex::m_thread_holding;
} // namespace credb
#endif