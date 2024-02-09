#include <cstdint>
#include "nrf_timer.hpp"
#include "nrf_delay.h"
#include "drv_rtc.h"
#include "interrupt_lock.hpp"
#include "bsp.hpp"
#include "debug.hpp"
#include "etl/list.h"

// Do not change this value without considering the impact to the macros below
static constexpr uint16_t RTC_TIMER_PRESCALER = 32;

// Note we can't get the 1000 Hz (1ms period) we'd like so we will get as close as we can:
// The RTC clock is 32768 Hz and the prescaler is 33 which yields 992.969696969697 Hz
// The following macros will converts between milliseconds and ticks using integer arithmetic
#define TICKS_TO_MS(ticks)  (((ticks) * 1000000ULL) / 992969ULL)
#define MS_TO_TICKS(ms)     (((ms) * 992969ULL) / 1000000ULL)

static constexpr uint32_t TICKS_PER_OVERFLOW = 16777216;
static constexpr uint32_t MILLISECONDS_PER_OVERFLOW = TICKS_TO_MS(TICKS_PER_OVERFLOW);

static volatile uint32_t g_overflows_occured;
static volatile uint64_t g_stamp64;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
struct Schedule
{
    stdext::inplace_function<void(), INPLACE_FUNCTION_SIZE_TIMER> m_func;
    std::optional<unsigned int> m_id;
    uint64_t m_target_ticks;
};
#pragma GCC diagnostic pop

static etl::list<Schedule, MAX_NUM_TIMERS> g_schedules;
static unsigned int g_unique_id;

// Return current 64 bit tick count
static uint64_t current_ticks()
{
    uint64_t now = drv_rtc_counter_get(&BSP::RTC_Inits[RTC_TIMER].rtc) + ((uint64_t)g_overflows_occured * (uint64_t)TICKS_PER_OVERFLOW);

    // It is possible that base was not updated and an overflow occured, in this case 'now' will be
    // 24bit value behind. An additional tick count updated on every 24 bit period is used to detect
    // that case. Apart from that 'now' should never be behind previously read tick count.

    if (now < g_stamp64) {
        now += TICKS_PER_OVERFLOW;
    }

    return now;
}

// This should only be run from our RTC interrupt context
static void setup_compare_interrupt()
{
    if (!g_schedules.size())
    {
        // There is nothing scheduled so ensure our compare0 interrupt will not fire
        drv_rtc_compare_disable(&BSP::RTC_Inits[RTC_TIMER].rtc, 0);
        return;
    }

    // Check to see if the next scheduled task will occur before the RTC overflow
    uint64_t next_counter_overflow_ticks = ((uint64_t)g_overflows_occured + 1) * (uint64_t)TICKS_PER_OVERFLOW;
    uint64_t next_schedule_ticks = g_schedules.front().m_target_ticks;

    // Ensure that we only set an alarm that is at least 5 ticks away from now
    // This is to ensure we do not set an alarm for a time that has already elapsed
    auto current_tick = current_ticks();
    if (next_schedule_ticks <= current_tick + 5)
        next_schedule_ticks = current_tick + 5;

    if (next_schedule_ticks < next_counter_overflow_ticks)
    {
        // This schedule will occcur before our RTC overflow interrupt
        // Because of this we need to schedule a new interrupt so we wake up earlier to service it
        uint32_t compare_value = next_schedule_ticks % TICKS_PER_OVERFLOW;
        drv_rtc_compare_set(&BSP::RTC_Inits[RTC_TIMER].rtc, 0, compare_value, true);
    }
}

static void rtc_time_keeping_event_handler(drv_rtc_t const * const  p_instance)
{
    //DEBUG_TRACE("rtc_time_keeping_event_handler: int_type=%u", int_type);

    if (drv_rtc_overflow_pending(p_instance))
    {
        g_overflows_occured = g_overflows_occured + 1;
    }
    else if (drv_rtc_compare_pending(p_instance, 0))
    {
        // Schedule/s must be due, so lets run them
        //printf("Woken by COMPARE0\n");

        auto schedule_itr = g_schedules.begin();
        while (schedule_itr != g_schedules.end())
        {
            Schedule schedule = *schedule_itr; 
            if (schedule.m_target_ticks <= current_ticks())
            {
                schedule_itr = g_schedules.erase(schedule_itr);

                //printf("Running schedule %u\n", (unsigned int)schedule.m_id.value());
                if (schedule.m_func)
                    schedule.m_func();
            }
            else
            {
                // As schedules are in time order we can exit early as an optimisation
                break;
            }
        }
    } else if (drv_rtc_compare_pending(p_instance, 1))
    {
        g_stamp64 = current_ticks();
    }

    setup_compare_interrupt();
}

void NrfTimer::init()
{
	m_start_ticks = 0;
    g_overflows_occured = 0;
    g_stamp64 = 0;

    drv_rtc_config_t rtc_config = {
        .prescaler          = RTC_TIMER_PRESCALER,
        .interrupt_priority = BSP::RTC_Inits[RTC_TIMER].irq_priority
    };

    drv_rtc_init(&BSP::RTC_Inits[RTC_TIMER].rtc, &rtc_config, rtc_time_keeping_event_handler);
    drv_rtc_compare_set(&BSP::RTC_Inits[RTC_TIMER].rtc, 1, RTC_COUNTER_COUNTER_Msk >> 1, true);
    drv_rtc_overflow_enable(&BSP::RTC_Inits[RTC_TIMER].rtc, true);
}

void NrfTimer::uninit()
{
    stop();

    // Wait for the RTC instance to have actually stopped
    nrf_delay_us(50);

    g_schedules.clear();
}

uint64_t NrfTimer::get_counter()
{
    // Current ticks is not atomic/thread-safe so we need to lock before calling it
    InterruptLock lock;
    uint64_t uptime = TICKS_TO_MS(current_ticks() - m_start_ticks);

    return uptime;
}

Timer::TimerHandle NrfTimer::add_schedule(stdext::inplace_function<void(), INPLACE_FUNCTION_SIZE_TIMER> const &task_func, uint64_t target_count_ms)
{
    // Create a schedule for this task
    Schedule schedule;
    uint64_t target_count_ticks = (MS_TO_TICKS(target_count_ms) + m_start_ticks);

    schedule.m_func = task_func;
    schedule.m_target_ticks = target_count_ticks;

    TimerHandle handle;

    {
        InterruptLock lock;

        // Assign global ID only when interrupt lock is held
        schedule.m_id = g_unique_id;

        // Add this schedule to our list in time order
        unsigned int index = 0;
        auto iter = g_schedules.begin();
        
        while (iter != g_schedules.end())
        {
            if (iter->m_target_ticks > target_count_ticks)
                break;
            iter++;
            index++;
        }
        g_schedules.insert(iter, schedule);

        // Generate a handle that refers to this new schedule
        handle = g_unique_id;

        //printf("Added schedule with id %u ticks %llu\n", *handle, target_count_ticks);

        g_unique_id++;
    }

    // Update our schedule
    drv_rtc_irq_trigger(&BSP::RTC_Inits[RTC_TIMER].rtc);

    return handle;
}

void NrfTimer::cancel_schedule(TimerHandle &handle)
{
    // Handle is invalid so can not be scheduled
    if (!handle.has_value())
        return;

    InterruptLock lock;

    // Find the given handle in our schedule list
    auto iter = g_schedules.begin();
    while (iter != g_schedules.end())
    {
        if (iter->m_id == *handle)
        {
            //printf("Cancelling schedule with id %u\n", *handle);
            iter = g_schedules.erase(iter);
            // Invalidate the task handle
            handle.reset();
            break;
        }
        else
        {
            iter++;
        }
    }

    // Update our schedule if we removed something
    if (iter != g_schedules.end())
        drv_rtc_irq_trigger(&BSP::RTC_Inits[RTC_TIMER].rtc);
}

void NrfTimer::start()
{
    m_start_ticks = current_ticks();
    DEBUG_TRACE("NrfTimer::start: ticks=%lu", (unsigned long)m_start_ticks);
    drv_rtc_start(&BSP::RTC_Inits[RTC_TIMER].rtc);
}

void NrfTimer::stop()
{
    drv_rtc_stop(&BSP::RTC_Inits[RTC_TIMER].rtc);
}
