#include "interrupt_lock.hpp"
#include "app_util_platform.h"

InterruptLock::InterruptLock()
{
#ifdef SOFTDEVICE_PRESENT
    m_nested = 0;
    app_util_critical_region_enter(&m_nested);
#else
    app_util_critical_region_enter(NULL);
#endif
}

InterruptLock::~InterruptLock()
{
#ifdef SOFTDEVICE_PRESENT
    app_util_critical_region_exit(m_nested);
#else
    app_util_critical_region_exit(0);
#endif
}
