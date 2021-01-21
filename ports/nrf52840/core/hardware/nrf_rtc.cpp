#include "nrf_rtc.hpp"
#include "nrfx_rtc.h"
#include "interrupt_lock.hpp"
#include "bsp.hpp"

// Timekeeping defines
static const uint32_t RTC_TIME_KEEPING_FREQUENCY_HZ = 8; // 8 Hz, this is the lowest speed available

static const int64_t TICKS_PER_OVERFLOW = 16777216; // 2 ^ 24
static const int64_t SECONDS_PER_OVERFLOW = (TICKS_PER_OVERFLOW / RTC_TIME_KEEPING_FREQUENCY_HZ);

static int64_t g_timestamp_offset;
static volatile uint32_t g_overflows_occured;

static void rtc_time_keeping_event_handler(nrfx_rtc_int_type_t int_type)
{
    if (NRFX_RTC_INT_OVERFLOW == int_type)
        g_overflows_occured++;
}

void NrfRTC::init()
{
    g_overflows_occured = 0;
    g_timestamp_offset = 0;

    const nrfx_rtc_config_t rtc_config =
    {
        .prescaler          = RTC_FREQ_TO_PRESCALER(RTC_TIME_KEEPING_FREQUENCY_HZ),
        .interrupt_priority = BSP::RTC_Inits[RTC_DATE_TIME].irq_priority,
        .tick_latency       = 0,
        .reliable           = false
    };

    nrfx_rtc_init(&BSP::RTC_Inits[RTC_DATE_TIME].rtc, &rtc_config, rtc_time_keeping_event_handler);
    nrfx_rtc_enable(&BSP::RTC_Inits[RTC_DATE_TIME].rtc);
    nrfx_rtc_overflow_enable(&BSP::RTC_Inits[RTC_DATE_TIME].rtc, true);
}

void NrfRTC::uninit()
{
    nrfx_rtc_uninit(&BSP::RTC_Inits[RTC_DATE_TIME].rtc);
}

int64_t NrfRTC::getuptime()
{
    InterruptLock lock;

    return static_cast<int64_t>(nrfx_rtc_counter_get(&BSP::RTC_Inits[RTC_DATE_TIME].rtc)) / RTC_TIME_KEEPING_FREQUENCY_HZ + (g_overflows_occured * SECONDS_PER_OVERFLOW);
}

std::time_t NrfRTC::gettime()
{
    InterruptLock lock;

    return getuptime() + g_timestamp_offset;
}

void NrfRTC::settime(std::time_t time)
{
    InterruptLock lock;

    g_timestamp_offset = static_cast<int64_t>(time) - getuptime();
}
