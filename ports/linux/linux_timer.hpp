#ifndef __LINUX_TIMER_HPP_
#define __LINUX_TIMER_HPP_

#include <iostream>
#include "timer.hpp"

extern "C" {
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
}

#define NS_PER_SEC    1000000000UL
#define MSEC          (NS_PER_SEC/1000)


using namespace std;

// This timer implementation just emulates a 1 ms tick based timer
// A real hardware implementation should ideally sleep the processor until the earliest
// timer schedule event arises rather than waking up every 1 ms period

class LinuxTimer : public Timer {
private:
	pthread_t m_pid;
	volatile bool m_is_running ;
	uint64_t m_counter_value;
	TimerSchedule m_schedule[MAX_TIMER_SCHEDULES];

	static uint64_t time_now() {
		uint64_t val;
		struct timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		val = (tp.tv_sec * NS_PER_SEC) + tp.tv_nsec;
		return val;
	}

	static void *timer_handler(void *arg) {

		LinuxTimer *parent = reinterpret_cast<LinuxTimer*>(arg);
		parent->m_is_running = true;
		uint64_t last_t;

		last_t = time_now();  // Start-up condition so we know initial high-res time value

		while (parent->m_is_running) {
			uint64_t t = time_now();

			unsigned int msec = ((t - last_t) / MSEC);

			// In some cases we may have elapsed more than 1 ms since the last call so we iterate the
			// number of milliseconds elapsed and call the scheduler for each tick that has elapsed

			for (unsigned int k = 0; k < msec; k++)
			{
				parent->m_counter_value++;
				//std::cout << "m_counter=" << m_counter_value << "\n";

				// Check the current schedule -- make a copy first since we may modify the original
				// vector size during the iteration process
				for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++) {
					TimerSchedule s = parent->m_schedule[i];
					if (s.event && s.target_counter_value == parent->m_counter_value) {
						parent->m_schedule[i] = {0, 0, 0};
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
	uint64_t get_counter() override {
		return m_counter_value;
	}
	void add_schedule(TimerSchedule &s) override {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i].event == 0) {
				m_schedule[i] = s;
				break;
			}
	}
	void cancel_schedule(TimerSchedule const &s) override {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i] == s) {
				m_schedule[i] = {0, 0, 0};
				break;
			}
	}
	void cancel_schedule(TimerEvent const &e) override {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i].event == e) {
				m_schedule[i] = {0, 0, 0};
				break;
			}
	}
	void cancel_schedule(void *user_arg) override {
		for (unsigned int i = 0; i < MAX_TIMER_SCHEDULES; i++)
			if (m_schedule[i].user_arg == user_arg) {
				m_schedule[i] = {0, 0, 0};
				break;
			}
	}
	void start() override {
		if (!m_is_running)
		{
			m_counter_value = 0;
			pthread_create(&m_pid, 0, timer_handler, this);
			//std::cout << "created" << "\n";
		}
	}
	void stop() override {
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

#endif // __LINUX_TIMER_HPP_
