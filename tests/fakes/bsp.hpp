#pragma once

#include <cstdint>
#include "nrfx_rtc.h"

#define RTC_TIMER      BSP::RTC::RTC_1

namespace BSP
{
    enum RTC
    {
        RTC_RESERVED, // Reserved by the softdevice
        RTC_1,
        RTC_2,
        RTC_TOTAL_NUMBER
    };

    typedef struct
    {
        nrfx_rtc_t rtc;
        uint8_t irq_priority;
    } RTC_InitTypeDefAndInst_t;

    extern const RTC_InitTypeDefAndInst_t RTC_Inits[RTC_TOTAL_NUMBER];
}