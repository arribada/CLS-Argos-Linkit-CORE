#include "interrupt_lock.hpp"
#include <mutex>

static std::mutex g_mtx;

InterruptLock::InterruptLock()
{
    g_mtx.lock();
}

InterruptLock::~InterruptLock()
{
    g_mtx.unlock();
}