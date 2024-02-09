#include "nrf_rtc.hpp"
#include "drv_rtc.h"
#include "interrupt_lock.hpp"
#include "bsp.hpp"

// Timekeeping defines
static const uint32_t RTC_TIME_KEEPING_FREQUENCY_HZ = 8; // 8 Hz, this is the lowest speed available

static const int64_t TICKS_PER_OVERFLOW = 16777216; // 2 ^ 24
static const int64_t SECONDS_PER_OVERFLOW = (TICKS_PER_OVERFLOW / RTC_TIME_KEEPING_FREQUENCY_HZ);

static int64_t g_timestamp_offset;
static volatile uint32_t g_overflows_occured;
static volatile uint64_t g_stamp64;

// Return current 64 bit tick count
static uint64_t current_ticks()
{
    uint64_t now = drv_rtc_counter_get(&BSP::RTC_Inits[RTC_DATE_TIME].rtc) + (g_overflows_occured * TICKS_PER_OVERFLOW);

    // It is possible that base was not updated and an overflow occured, in this case 'now' will be
    // 24bit value behind. An additional tick count updated on every 24 bit period is used to detect
    // that case. Apart from that 'now' should never be behind previously read tick count.

    if (now < g_stamp64) {
        now += TICKS_PER_OVERFLOW;
    }

    return now;
}

static void rtc_time_keeping_event_handler(drv_rtc_t const * const  p_instance)
{
    if (drv_rtc_overflow_pending(p_instance))
        g_overflows_occured = g_overflows_occured + 1;
    else if (drv_rtc_compare_pending(p_instance, 0))
        g_stamp64 = current_ticks();
}

void NrfRTC::init()
{
    m_is_set = false;
    g_overflows_occured = 0;
    g_timestamp_offset = 0;
    g_stamp64 = 0;

    const drv_rtc_config_t rtc_config = {
        .prescaler          = RTC_FREQ_TO_PRESCALER(RTC_TIME_KEEPING_FREQUENCY_HZ),
        .interrupt_priority = BSP::RTC_Inits[RTC_DATE_TIME].irq_priority
    };

    drv_rtc_init(&BSP::RTC_Inits[RTC_DATE_TIME].rtc, &rtc_config, rtc_time_keeping_event_handler);
    drv_rtc_overflow_enable(&BSP::RTC_Inits[RTC_DATE_TIME].rtc, true);
    drv_rtc_compare_set(&BSP::RTC_Inits[RTC_DATE_TIME].rtc, 0, RTC_COUNTER_COUNTER_Msk >> 1, true);
    drv_rtc_start(&BSP::RTC_Inits[RTC_DATE_TIME].rtc);
}

void NrfRTC::uninit()
{
    drv_rtc_stop(&BSP::RTC_Inits[RTC_DATE_TIME].rtc);
}

int64_t NrfRTC::getuptime()
{
    InterruptLock lock;

    return current_ticks() / RTC_TIME_KEEPING_FREQUENCY_HZ;
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
    m_is_set = true;
}

bool NrfRTC::is_set() {
	return m_is_set;
}
