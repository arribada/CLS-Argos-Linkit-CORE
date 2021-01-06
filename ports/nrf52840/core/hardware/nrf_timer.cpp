#include <atomic>
#include <cstdint>
#include "nrf_timer.hpp"
#include "nrfx_rtc.h"
#include "interrupt_lock.hpp"
#include "bsp.hpp"

// Note we can't get the 1000 Hz (1ms period) we'd like so we will get as close as we can
static constexpr uint16_t RTC_TIMER_PRESCALER = 32;
static constexpr double RTC_TIMER_FREQ = (RTC_INPUT_FREQ / (RTC_TIMER_PRESCALER + 1.0f));
static constexpr double MS_PER_TICK = ((RTC_TIMER_PRESCALER + 1.0f) * 1000.0f) / RTC_INPUT_FREQ;

static constexpr uint32_t TICKS_PER_OVERFLOW = 16777216;
static constexpr uint32_t MILLISECONDS_PER_OVERFLOW = TICKS_PER_OVERFLOW * MS_PER_TICK;

//static volatile uint32_t g_overflows_occured;
static std::atomic<uint32_t> g_overflows_occured;
static_assert(std::atomic<uint32_t>::is_always_lock_free);

struct Schedule
{
    std::function<void()> m_func;
    std::optional<unsigned int> m_id;
    uint64_t m_target_ms;
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
    uint64_t next_counter_overflow = (g_overflows_occured + 1) * MILLISECONDS_PER_OVERFLOW;
    //uint64_t current_time = get_counter();
    uint64_t next_schedule = g_schedules.front().m_target_ms;
    
    if (next_schedule < next_counter_overflow)
    {
        // This schedule will occcur before our RTC overflow interrupt
        // Because of this we need to schedule a new interrupt so we wake up earlier to service it

        uint32_t compare_value = static_cast<uint64_t>(next_schedule / MS_PER_TICK) % TICKS_PER_OVERFLOW;
        nrfx_rtc_cc_set(&BSP::RTC_Inits[RTC_TIMER].rtc, 0, compare_value, true);
    }
}

static void rtc_time_keeping_event_handler(nrfx_rtc_int_type_t int_type)
{
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
    uptime = nrfx_rtc_counter_get(&BSP::RTC_Inits[RTC_TIMER].rtc) * MS_PER_TICK + (g_overflows_occured * MILLISECONDS_PER_OVERFLOW);

    return uptime;
}

Timer::TimerHandle NrfTimer::add_schedule(std::function<void()> const &task_func, uint64_t target_count)
{
    // Convert target_count to our internal counting timebase
    target_count *= MS_PER_TICK;

    // Create a schedule for this task
    Schedule schedule;

    schedule.m_id = g_unique_id;
    schedule.m_func = task_func;
    schedule.m_target_ms = target_count;

    InterruptLock lock;

    // Add this schedule to our list in time order
    auto iter = g_schedules.begin();
    bool new_schedule_first = true;
    while (iter != g_schedules.end())
    {
        if (iter->m_target_ms > target_count)
            break;
        iter++;
        new_schedule_first = false;
    }
    g_schedules.insert(iter, schedule);

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
