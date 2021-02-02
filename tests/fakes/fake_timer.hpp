
#pragma once

#include <iostream>
#include <list>
#include "timer.hpp"

class FakeTimer : public Timer {
public:
	FakeTimer() : m_counter_value(0), m_is_running(false), m_unique_id(0) {}
	~FakeTimer() {
		stop();
	}

	void start() override
	{
		if (!m_is_running)
		{
			m_is_running = true;
			m_counter_value = 0;
		}
	}

	void stop() override
	{
		if (m_is_running)
		{
			m_is_running = false;
			m_schedules.clear();
		}
	}

	uint64_t get_counter() override
	{
		return m_counter_value;
	}

    void increment_counter(uint64_t t)
    {
        while (t)
        {
            t--;
            m_counter_value++;

            process_schedules();
        }
    }

	TimerHandle add_schedule(std::function<void()> const &task_func, uint64_t target_count) override
	{
		Schedule schedule;;

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

		//std::cout << "Created schedule with id: " << m_unique_id << std::endl;

		m_unique_id++;

		return handle;
	}

	void cancel_schedule(TimerHandle &handle) override
	{
		if (!handle.has_value())
			return;

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

    void process_schedules()
    {
        // Check to see if any schedules are due and run any that are
        for (auto itr = m_schedules.begin(); itr != m_schedules.end(); )
        {
            if (itr->m_target_counter_value == m_counter_value)
            {
                if (itr->m_func)
                {
                    itr->m_func();
                    itr = m_schedules.erase(itr);
                    continue;
                }
            }

            itr++;
        }
    }

	uint64_t m_counter_value;
	bool m_is_running;

	struct Schedule
	{
		std::function<void()> m_func;
		std::optional<unsigned int> m_id;
		uint64_t m_target_counter_value;
	};
	
	std::list<Schedule> m_schedules;

	unsigned int m_unique_id;
};
