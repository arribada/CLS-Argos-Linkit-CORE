#include <atomic>
#include <cstdint>
#include "nrf_timer.hpp"
#include "nrfx_rtc.h"
#include "interrupt_lock.hpp"
#include "bsp.hpp"
#include "debug.hpp"

// Do not change this value without considering the impact to the macros below
static constexpr uint16_t RTC_TIMER_PRESCALER = 32;

// Note we can't get the 1000 Hz (1ms period) we'd like so we will get as close as we can:
// The RTC clock is 32768 Hz and the prescaler is 33 which yields 992.969696969697 Hz
// The following macros will converts between milliseconds and ticks using integer arithmetic
#define TICKS_TO_MS(ticks)  ((ticks) * 1000000ULL) / 992969ULL;
#define MS_TO_TICKS(ms)     ((ms) * 992969ULL) / 1000000ULL;

static constexpr uint32_t TICKS_PER_OVERFLOW = 16777216;
static constexpr uint32_t MILLISECONDS_PER_OVERFLOW = TICKS_TO_MS(TICKS_PER_OVERFLOW);

//static volatile uint32_t g_overflows_occured;
static std::atomic<uint32_t> g_overflows_occured;
static_assert(std::atomic<uint32_t>::is_always_lock_free);

struct Schedule
{
    std::function<void()> m_func;
    std::optional<unsigned int> m_id;
    uint64_t m_target_ticks;
};

static std::list<Schedule> g_schedules;
static unsigned int g_unique_id;

// If calling this function it is advisable to disable interrupts using an InterruptLock
static void setup_compare_interrupt()
{
    if (!g_schedules.size())
    {
        // There is nothing scheduled so ensure our compare0 interrupt will not fire
        nrfx_rtc_cc_disable(&BSP::RTC_Inits[RTC_TIMER].rtc, 0);
        return;
    }

    // Check to see if the next scheduled task will occur before the RTC overflow
    uint64_t next_counter_overflow_ticks = (g_overflows_occured + 1) * TICKS_PER_OVERFLOW;
    //uint64_t current_time_ticks = nrfx_rtc_counter_get(&BSP::RTC_Inits[RTC_TIMER].rtc) + (g_overflows_occured * TICKS_PER_OVERFLOW);
    uint64_t next_schedule_ticks = g_schedules.front().m_target_ticks;

    //DEBUG_TRACE("setup_compare_interrupt: next_counter_overflow_ticks=%llu current_time_ticks=%llu next_schedule_ticks=%llu",
    //		next_counter_overflow_ticks, current_time_ticks, next_schedule_ticks);

    if (next_schedule_ticks < next_counter_overflow_ticks)
    {
        // This schedule will occcur before our RTC overflow interrupt
        // Because of this we need to schedule a new interrupt so we wake up earlier to service it
        uint32_t compare_value = next_schedule_ticks % TICKS_PER_OVERFLOW;
        nrfx_rtc_cc_set(&BSP::RTC_Inits[RTC_TIMER].rtc, 0, compare_value, true);
    }
}

static void rtc_time_keeping_event_handler(nrfx_rtc_int_type_t int_type)
{
    //DEBUG_TRACE("rtc_time_keeping_event_handler: int_type=%u", int_type);

    if (NRFX_RTC_INT_OVERFLOW == int_type)
    {
        g_overflows_occured++;
        setup_compare_interrupt();
    }
    else if (NRFX_RTC_INT_COMPARE0 == int_type)
    {
        // A schedule must be due, so lets run it

        if (g_schedules.size())
        {
            Schedule schedule = g_schedules.front();
            g_schedules.pop_front();

            if (schedule.m_func)
                schedule.m_func();
        }

        //printf("Woken by COMPARE0\r\n");

        setup_compare_interrupt();
    }
}

void NrfTimer::init()
{
    g_overflows_occured = 0;

    const nrfx_rtc_config_t rtc_config =
    {
        .prescaler          = RTC_TIMER_PRESCALER,
        .interrupt_priority = BSP::RTC_Inits[RTC_TIMER].irq_priority,
        .tick_latency       = 0,
        .reliable           = false
    };

    nrfx_rtc_init(&BSP::RTC_Inits[RTC_TIMER].rtc, &rtc_config, rtc_time_keeping_event_handler);
    nrfx_rtc_overflow_enable(&BSP::RTC_Inits[RTC_TIMER].rtc, true);
}

void NrfTimer::uninit()
{
    nrfx_rtc_uninit(&BSP::RTC_Inits[RTC_TIMER].rtc);

    g_schedules.clear();
}

uint64_t NrfTimer::get_counter()
{
    uint64_t uptime;

    InterruptLock lock;
    uptime = TICKS_TO_MS(nrfx_rtc_counter_get(&BSP::RTC_Inits[RTC_TIMER].rtc) + (g_overflows_occured * TICKS_PER_OVERFLOW));

    return uptime;
}

Timer::TimerHandle NrfTimer::add_schedule(std::function<void()> const &task_func, uint64_t target_count_ms)
{
    // Create a schedule for this task
    Schedule schedule;
    uint64_t target_count_ticks = MS_TO_TICKS(target_count_ms);

    schedule.m_id = g_unique_id;
    schedule.m_func = task_func;
    schedule.m_target_ticks = target_count_ticks;

    InterruptLock lock;

    // Add this schedule to our list in time order
    unsigned int index = 0;
    auto iter = g_schedules.begin();
    bool new_schedule_first = true;
    while (iter != g_schedules.end())
    {
        if (iter->m_target_ticks > target_count_ticks)
            break;
        iter++;
        index++;
        new_schedule_first = false;
    }
    g_schedules.insert(iter, schedule);

	//DEBUG_TRACE("NrfTimer::add_schedule: #g_schedules=%u index=%u is_first=%u", g_schedules.size(), index, new_schedule_first);

    // If this task will occur before any others then we may need to set an alarm to service it next
    if (new_schedule_first)
        setup_compare_interrupt();

    // Generate a handle that refers to this new schedule
    TimerHandle handle;
    handle = g_unique_id;

    g_unique_id++;

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
    bool front_removed = true;
    while (iter != g_schedules.end())
    {
        if (iter->m_id == *handle)
        {
            //std::cout << "Cancelling schedule with id: " << *handle << std::endl;
            iter = g_schedules.erase(iter);
            // Invalidate the task handle
            handle.reset();
            break;
        }
        else
        {
            iter++;
            front_removed = false;
        }
    }

    // If we removed our first task we will need to update our next potential interrupt time
    if (front_removed)
        setup_compare_interrupt();

    // Probably an error if we get to here without returning
    //std::cout << "Tried to cancel schedule with id: " << *handle << " which did not exist in the timer schedule" << std::endl;
}

void NrfTimer::start()
{
    nrfx_rtc_enable(&BSP::RTC_Inits[RTC_TIMER].rtc);
}

void NrfTimer::stop()
{
    nrfx_rtc_disable(&BSP::RTC_Inits[RTC_TIMER].rtc);
}
