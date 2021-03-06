#pragma once

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
	static const unsigned int HIGHEST_PRIORITY = 0;
	static const unsigned int DEFAULT_PRIORITY = 7;

	static constexpr size_t EXPECTED_MAX_TASKS = 16;

	Scheduler(Timer *timer) : m_timer(timer), m_unique_id(0) {
		m_timer_schedules.reserve(EXPECTED_MAX_TASKS);
	}
	Scheduler(const Scheduler &) = delete;
	
	class Task 
    {
		friend class Scheduler;

	private:
		const char *m_name;
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
		const char *m_name;
		Scheduler *m_parent;
		std::optional<unsigned int> m_id;
	};

	// Used for queuing a static or free function as a task
	TaskHandle post_task_prio(std::function<void()> const &task_func, const char *task_name, unsigned int priority = DEFAULT_PRIORITY, unsigned int delay_ms = 0) {

		Task task;
		task.m_priority = priority;
		task.m_func = task_func;
		task.m_name = task_name;

		// Safely create a new unique ID for this task
		{
			InterruptLock lock;
			task.m_id = m_unique_id++; // Incrementing this is non-atomic so we must do so within a lock

			if (!delay_ms)
				schedule_now(task);
			else
				schedule_deferred(task, delay_ms);
		}

		TaskHandle handle;
		handle.m_id = task.m_id;
		handle.m_parent = this;
		handle.m_name = task_name;

#ifdef SCHEDULER_DEBUG
		DEBUG_TRACE("Scheduler: post_task_prio: added %s", task_name);
#endif
		return handle;
	}

	void cancel_task(TaskHandle &task) {

		if (task.m_parent != this) {
			return; // This handle belongs to another scheduler
		}

		if (!task.m_id.has_value()) {
			return;
		}

		InterruptLock lock;

		// Check to see if this task is in our immediate task list
		auto iter = m_tasks.begin();
		while (iter != m_tasks.end())
		{
			if (iter->m_id == task.m_id)
			{
#ifdef SCHEDULER_DEBUG
				DEBUG_TRACE("Scheduler: cancel_task: %s [immediate]", task.m_name);
#endif
				iter = m_tasks.erase(iter);
				break;
			}
			else
				iter++;
		}

		// Check if this task is in our deferred task list
		// If so then cancel the timer scheduler for it
		auto schedule = m_timer_schedules.find(*task.m_id);
		if (schedule != m_timer_schedules.end())
		{
#ifdef SCHEDULER_DEBUG
			DEBUG_TRACE("Scheduler: cancel_task: %s [timer]", task.m_name);
#endif
			m_timer->cancel_schedule(schedule->second);
			m_timer_schedules.erase(schedule);
		}

		// Invalidate the task handle in all cases
		task.m_id.reset();
		task.m_parent = nullptr;
	}

	bool run() {

		// Run through our queue of tasks in order and run them
		// As our queue is in priority order so will our run order
		bool tasks_ran = false;

		while (true)
		{
			// We need to safetly retrieve the front value with a lock
			// We then need to release the lock so that the called function may add/cancel tasks

			Task task;
			{
				InterruptLock lock;
				if (!m_tasks.size())
					return tasks_ran;

				task = m_tasks.front();
				m_tasks.pop_front();
			}

			if (task.m_func) {
#ifdef SCHEDULER_DEBUG
				DEBUG_TRACE("Scheduler: run: %s", task.m_name);
#endif
				tasks_ran = true;
				task.m_func();
			}
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
	
	bool is_any_task_scheduled()
	{
		InterruptLock lock;
		return m_tasks.size() + m_timer_schedules.size();
	}

private:

	void schedule_deferred(Task task, unsigned int delay_ms) {
		InterruptLock lock;
		// If this task was delayed then schedule a timer to start it
		uint64_t t_sched = m_timer->get_counter() + delay_ms;

		// We do this by setting up our timer to call this function after the delay has elapsed
		m_timer_schedules[*task.m_id] = m_timer->add_schedule([this, task]() {
			m_timer_schedules.erase(*task.m_id);
			this->schedule_now(task);
		}, t_sched);
	}

	void schedule_now(Task task) {
		InterruptLock lock;
		// Task is requested to be processed on next run()
		// Safetly add this task to our task list in priority order
		auto iter = m_tasks.begin();
		while (iter != m_tasks.end())
		{
			if (iter->m_priority > task.m_priority)
				break;
			iter++;
		}
		m_tasks.insert(iter, task);
	}

	std::list<Task> m_tasks;
	std::unordered_map<unsigned int, Timer::TimerHandle> m_timer_schedules;
	Timer *m_timer;
	unsigned int m_unique_id;
};
