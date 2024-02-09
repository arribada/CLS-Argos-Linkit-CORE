#include "heap.h"
#include "app_util_platform.h"

static uint8_t g_nested = 0;

void vTaskSuspendAll(void)
{
#ifdef SOFTDEVICE_PRESENT
    g_nested = 0;
    app_util_critical_region_enter(&g_nested);
#else
    app_util_critical_region_enter(NULL);
#endif
}

void xTaskResumeAll(void)
{
#ifdef SOFTDEVICE_PRESENT
    app_util_critical_region_exit(g_nested);
#else
    app_util_critical_region_exit(0);
#endif
}