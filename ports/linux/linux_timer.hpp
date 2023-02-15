
#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <list>
#include "timer.hpp"

class LinuxTimer : public Timer {
public:
	LinuxTimer() : m_counter_value(0), m_is_running(false), m_unique_id(0) {}
	~LinuxTimer() {
		stop();
	}

	void start() override
	{
		if (!m_is_running)
		{
			m_is_running = true;
			m_counter_value = 0;
			m_counter_thread = std::thread(timer_thread_func, this);
		}
	}

	void stop() override
	{
		if (m_is_running)
		{
			m_is_running = false;
			m_counter_thread.join();
			m_schedules.clear();
		}
	}

	uint64_t get_counter() override
	{
		return m_counter_value;
	}

	void set_counter(uint64_t t)
	{
		m_counter_value = t;
	}

	TimerHandle add_schedule(stdext::inplace_function<void(), INPLACE_FUNCTION_SIZE_TIMER> const &task_func, uint64_t target_count) override
	{
		Schedule schedule;

		std::lock_guard<std::mutex> lock(m_mtx_map);

		schedule.m_id = m_unique_id;
		schedule.m_func = task_func;
		schedule.m_target_counter_value = target_count;

		// Add this schedule to our list in time order
		auto iter = m_schedules.begin();
		while (iter != m_schedules.end())
		{
			if (iter->m_target_counter_value > target_count)
				break;
			iter++;
		}
		m_schedules.insert(iter, schedule);

		TimerHandle handle;
		handle = m_unique_id;

		//std::cout << "Created schedule with id: " << m_unique_id << " target_count: " << std::dec << target_count << std::endl;

		m_unique_id++;

		return handle;
	}

	void cancel_schedule(TimerHandle &handle) override
	{
		if (!handle.has_value())
			return;

		std::lock_guard<std::mutex> lock(m_mtx_map);

		// Find the given handle in our schedule list
		auto iter = m_schedules.begin();
		while (iter != m_schedules.end())
		{
			if (iter->m_id == *handle)
			{
				//std::cout << "Cancelling schedule with id: " << *handle << std::endl;
				iter = m_schedules.erase(iter);
				// Invalidate the task handle
				handle.reset();
				return;
			}
			else
				iter++;
		}

		// Probably an error if we get to here without returning
		//std::cout << "Tried to cancel schedule with id: " << *handle << " which did not exist in the timer schedule" << std::endl;
	}

private:
	static uint64_t time_now_ms() {
		uint64_t val;
		struct timespec tp;
		clock_gettime(CLOCK_MONOTONIC, &tp);
		val = (tp.tv_sec * NS_PER_SEC) + tp.tv_nsec;
		return val;
	}

	static void timer_thread_func(LinuxTimer *parent) {
		uint64_t last_t;

		last_t = time_now_ms();  // Start-up condition so we know initial high-res time value

		while (parent->m_is_running) {
			uint64_t t = time_now_ms();

			unsigned int msec = ((t - last_t) / MSEC);

			// In some cases we may have elapsed more than 1 ms since the last call so we iterate the
			// number of milliseconds elapsed and call the scheduler for each tick that has elapsed

			for (unsigned int k = 0; k < msec; k++)
			{
				parent->m_counter_value++;

				//std::cout << "m_counter_value=" << std::dec << parent->m_counter_value << std::endl;

				// Check the current schedule for any functions that are due to be called
				while (true)
				{
					// We need to safetly retrieve the front value with a lock
					// We then need to release the lock so that the called function may add/cancel schedules
					Schedule schedule;
					bool execute_this_schedule = false;
					{
						std::lock_guard<std::mutex> lock(parent->m_mtx_map);

						if (!parent->m_schedules.size())
							break;
						
						schedule = parent->m_schedules.front();

						//std::cout << "m_target_counter_value=" << std::dec << schedule.m_target_counter_value << std::endl;

						if (schedule.m_target_counter_value == parent->m_counter_value)
						{
							execute_this_schedule = true;
							parent->m_schedules.pop_front(); // Remove this schedule from our list
						}
						else
						{
							// If the front schedule is not due to be run then we can assume none of them are
							// This is because the list is sorted
							break;
						}
					}

					if (execute_this_schedule)
					{
						//std::cout << "Running schedule with id: " << *schedule.m_id << std::endl;
						if (schedule.m_func)
							schedule.m_func();
					}
				}

				last_t = t;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Assumes 1 ms timer tick but it is not guaranteed to wake-up after 1 ms
		}
	}

	static constexpr unsigned long NS_PER_SEC = 1000000000UL;
	static constexpr unsigned long MSEC       = (NS_PER_SEC/1000);

	std::thread m_counter_thread;
	std::atomic<uint64_t> m_counter_value;
	std::atomic<bool> m_is_running;

	struct Schedule
	{
		std::function<void()> m_func;
		std::optional<unsigned int> m_id;
		uint64_t m_target_counter_value;
	};
	
	std::list<Schedule> m_schedules;
	std::mutex m_mtx_map;

	unsigned int m_unique_id;
};
