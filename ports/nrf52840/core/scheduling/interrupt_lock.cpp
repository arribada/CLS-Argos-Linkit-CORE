#include "interrupt_lock.hpp"
#include "app_util_platform.h"

#ifndef SOFTDEVICE_PRESENT
static uint32_t g_in_critical_region = 0;
#endif

InterruptLock::InterruptLock()
{
#ifdef SOFTDEVICE_PRESENT
    m_nested = 0;
    sd_nvic_critical_region_enter(&m_nested);
#else
    __disable_irq();
    g_in_critical_region++;
#endif
}

InterruptLock::~InterruptLock()
{
#ifdef SOFTDEVICE_PRESENT
    sd_nvic_critical_region_exit(m_nested);
#else
    g_in_critical_region--;
    if (g_in_critical_region == 0)
        __enable_irq();
#endif
}
