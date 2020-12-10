#include "interrupt_lock.hpp"
#include "app_util_platform.h"

InterruptLock::InterruptLock()
{
    CRITICAL_REGION_ENTER();
}

InterruptLock::~InterruptLock()
{
    CRITICAL_REGION_EXIT();
}