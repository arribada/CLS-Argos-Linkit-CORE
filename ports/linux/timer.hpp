#ifndef __TIMER_HPP_
#define __TIMER_HPP_

#include <vector>
#include <algorithm>
#include <iostream>

extern "C" {
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
}

#define NS_PER_SEC    1000000000UL
#define MSEC          (NS_PER_SEC/1000)

#define MAX_TIMER_SCHEDULES    32


using namespace std;

using TimerEvent = void(*)(void *);

struct TimerSchedule {
	void *user_arg;
	TimerEvent event;
	uint64_t target_counter_value;
};

static bool operator==(const TimerSchedule& lhs, const TimerSchedule& rhs)
{
    return (lhs.user_arg == rhs.user_arg &&
    		lhs.event == rhs.event &&
			lhs.target_counter_value == rhs.target_counter_value);
}

// This timer implementation just emulates a 1 ms tick based timer
// A real hardware implementation should ideally sleep the processor until the earliest
// timer schedule event arises rather than waking up every 1 ms period

class Timer {
private:
	static inline pthread_t m_pid;
	static inline volatile bool m_is_running = false;
	static inline uint64_t m_counter_value = 0;
	static inline TimerSchedule m_schedule[MAX_TIMER_SCHEDULES];

	static uint64_t time_now() {
		uint64_t val;
		struct timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		val = (tp.tv_sec * NS_PER_SEC) + tp.tv_nsec;
		return val;
	}

	static void *timer_handler(void *arg) {

		m_is_running = true;

		(void)arg;

		uint64_t last_t;

		last_t = time_now();  // Start-up condition so we know initial high-res time value

		while (m_is_running) {
			uint64_t t = time_now();

			unsigned int msec = ((t - last_t) / MSEC);

			// In some cases we may have elapsed more than 1 ms since the last call so we iterate the
			// number of milliseconds elapsed and call the scheduler for each tick that has elapsed

			for (unsigned int k = 0; k < msec; k++)
			{
				m_counter_value++;
				//std::cout << "m_counter=" << m_counter_value << "\n";

				// Check the current schedule -- make a copy first since we may modify the original
				// vector size during the iteration process
				for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++) {
					TimerSchedule s = m_schedule[i];
					if (s.event && s.target_counter_value == m_counter_value) {
						m_schedule[i] = {0, 0, 0};
						s.event(s.user_arg);
					}
				}

				last_t = t;
			}

			usleep(1000);   // Assumes 1 ms timer tick but it is not guaranteed to wake-up after 1 ms
		}

		return 0;
	}

public:
	static uint64_t get_counter() {
		return m_counter_value;
	}
	static void add_schedule(TimerSchedule &s) {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i].event == 0) {
				m_schedule[i] = s;
				break;
			}
	}
	static void cancel_schedule(TimerSchedule const &s) {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i] == s) {
				m_schedule[i] = {0, 0, 0};
				break;
			}
	}
	static void cancel_schedule(TimerEvent const &e) {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i].event == e) {
				m_schedule[i] = {0, 0, 0};
				break;
			}
	}
	static void cancel_schedule(void *user_arg) {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i].user_arg == user_arg) {
				m_schedule[i] = {0, 0, 0};
				break;
			}
	}
	static void start() {
		if (!m_is_running)
		{
			m_counter_value = 0;
			pthread_create(&m_pid, 0, timer_handler, 0);
			//std::cout << "created" << "\n";
		}
	}
	static void stop() {
		if (m_is_running)
		{
			m_is_running = false;
			pthread_join(m_pid, 0);
			//std::cout << "stopped" << "\n";
			for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
				m_schedule[i] = {0, 0, 0};
		}
	}

};

#endif // __TIMER_HPP_
