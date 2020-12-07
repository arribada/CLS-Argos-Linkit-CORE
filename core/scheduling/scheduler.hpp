#ifndef __SCHEDULER_HPP_
#define __SCHEDULER_HPP_

#include <functional>
#include <algorithm>
#include <optional>
#include <memory>
#include <map>
#include <list>

#include "interrupt_lock.hpp"
#include "timer.hpp"
#include "pmu.hpp"
#include "debug.hpp"

class Scheduler {

public:
	static const unsigned int DEFAULT_PRIORITY = 7;

	Scheduler(Timer *timer) : m_timer(timer), m_unique_id(0) {}
	Scheduler(const Scheduler &) = delete;
	
	class Task 
    {
		friend class Scheduler;

	private:
		std::optional<unsigned int> m_id;
		unsigned int m_priority;
		std::function<void()> m_func;
    };
	class TaskHandle
	{
		friend class Scheduler;
	
	public:
		TaskHandle() : m_parent(nullptr) {}

	private:
		Scheduler *m_parent;
		std::optional<unsigned int> m_id;
	};

	// Used for queuing a static or free function as a task
	TaskHandle post_task_prio(std::function<void()> const &task_func, unsigned int priority = DEFAULT_PRIORITY, unsigned int delay_ms = 0) {

		Task task;
		task.m_id = m_unique_id++;
		task.m_priority = priority;
		task.m_func = task_func;

		if (!delay_ms)
		{
			// Task is requested to be processed on next run()
			// Safetly add this task to our task list in priority order
			{
				InterruptLock lock;
				auto iter = m_tasks.begin();
				while (iter != m_tasks.end())
				{
					if (iter->m_priority > priority)
						break;
					iter++;
				}
				m_tasks.insert(iter, task);
			}
		}
		else
		{
			// If this task was delayed then schedule a timer to start it
			{
				InterruptLock lock;
				// We do this by setting up our timer to call this function after the delay has elapsed
				m_timer_schedules[*task.m_id] = m_timer->add_schedule([this, task_func, priority]() { this->post_task_prio(task_func, priority, 0); }, m_timer->get_counter() + delay_ms);
			}
		}

		TaskHandle handle;
		handle.m_id = task.m_id;
		handle.m_parent = this;

		return handle;
	}

	void cancel_task(TaskHandle &task) {
		if (task.m_parent != this)
			return; // This handle belongs to another scheduler

		if (!task.m_id.has_value())
			return;
		
		InterruptLock lock;

		// Check to see if this task is in our immediate task list
		auto iter = m_tasks.begin();
		while (iter != m_tasks.end())
		{
			if (iter->m_id == task.m_id)
			{
				iter = m_tasks.erase(iter);
				// Invalidate the task handle
				task.m_id.reset();
				task.m_parent = nullptr;
				return;
			}
			else
				iter++;
		}

		// Check if this task is in our deferred task list
		// If so then cancel the timer scheduler for it
		auto schedule = m_timer_schedules.find(*task.m_id);
		if (schedule != m_timer_schedules.end())
		{
			m_timer->cancel_schedule(schedule->second);
			m_timer_schedules.erase(schedule);
		}
	}

	void run(std::function<void(int)> const &exception_handler) {

		// Run through our queue of tasks in order and run them
		// As our queue is in priority order so will our run order

		while (true)
		{
			// We need to safetly retrieve the front value with a lock
			// We then need to release the lock so that the called function may add/cancel tasks

			Task task;
			{
				InterruptLock lock;
				if (!m_tasks.size())
					return;

				task = m_tasks.front();
				m_tasks.pop_front();
			}

			if (task.m_func)
				task.m_func();
		}
	}

	bool is_scheduled(TaskHandle task) {
		if (task.m_parent != this)
			return false; // This handle belongs to another scheduler

		if (!task.m_id.has_value())
			return false;
		
		InterruptLock lock;

		// Check to see if this task is in our immediate task list
		auto iter = m_tasks.begin();
		while (iter != m_tasks.end())
		{
			if (iter->m_id == task.m_id)
			{
				return true;
			}
			else
				iter++;
		}

		// Check if this task is in our deferred task list
		auto schedule = m_timer_schedules.find(*task.m_id);
		if (schedule != m_timer_schedules.end())
			return true;

		return false;
	}

	void clear_all()
	{
		InterruptLock lock;
		m_tasks.clear();

		// Cancel all scheduled tasks
		auto iter = m_timer_schedules.begin();
		while (iter != m_timer_schedules.end())
		{
			m_timer->cancel_schedule(iter->second);
			iter = m_timer_schedules.erase(iter);
		}
	}
	
private:

	std::list<Task> m_tasks;
	std::unordered_map<unsigned int, Timer::TimerHandle> m_timer_schedules;
	Timer *m_timer;
	unsigned int m_unique_id;
};

#endif // __SCHEDULER_HPP_
