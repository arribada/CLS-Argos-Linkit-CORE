#ifndef __SCHEDULER_HPP_
#define __SCHEDULER_HPP_

#include <map>

#include "timer.hpp"
#include "pmu.hpp"
#include "debug.hpp"

using namespace std;

using Task = void (*)();

struct TaskInfo {
	int  	prio;
	bool 	runnable;
};


class Scheduler {
public:
	static inline const int MIN_PRIORITY = 7;
	static inline const int MAX_PRIORITY = 1;
	static inline const int DEFAULT_PRIORITY = MIN_PRIORITY;

	Scheduler(Timer *timer) {
		m_timer = timer;
	}

	void run(void (*exception_handler)(int), int iter=-1) {
		DEBUG_TRACE("Scheduler.run: iter: %u", iter);
		while (iter > 0 || iter == -1) {
			iter--;
			try {
				Task runnable_task;
				do {
					// Find the highest priority runnable task each time we iterate this loop until
					// there are no more runnable tasks
					runnable_task = 0;
					for ( const auto &task : m_task_map ) {
						if (runnable_task && task.second.runnable && m_task_map[runnable_task].prio > task.second.prio) {
							runnable_task = task.first; // This task is higher priority
						} else if (!runnable_task && task.second.runnable) {
							runnable_task = task.first; // First runnable task found
						}
					}

					// Execute the highest priority runnable task
					if (runnable_task)
					{
						DEBUG_TRACE("Scheduler: running task %p", runnable_task);
						m_task_map[runnable_task] = {0, false};
						runnable_task();
					}
				} while (runnable_task);
			} catch (int e) {
				exception_handler(e);
			}

			PMU::enter_low_power_mode();
		}
	}
	void post_task_prio(Task task, int prio=Scheduler::DEFAULT_PRIORITY, unsigned int delay_ms=0) {
		DEBUG_TRACE("post_task_prio(%p)", task);
		m_timer->cancel_schedule((void *)task);  // To be safe, delete any existing timer event for this task
		if (delay_ms == 0) {
			// Mark as runnable immediately
			m_task_map[task] = { prio, true };
		} else {
			// Defer execution by invoking the timer to tell us when the scheduled delay period arrives
			m_task_map[task] = { prio, false };
			TimerSchedule s = { (void *)&m_task_map[task], timer_event, m_timer->get_counter() + delay_ms };
			m_timer->add_schedule(s);
		}
	}
	void cancel_task(Task task) {
		m_timer->cancel_schedule((void *)&m_task_map[task]);
		m_task_map[task] = { 0, false };
	}
	bool is_scheduled(Task task) {
		return (m_task_map[task].prio > 0);
	}

private:
	// Tracks all scheduled tasks, their priority and if they are runnable or not
	std::map<Task, TaskInfo> m_task_map;
	Timer *m_timer;

	static void timer_event(void *user_arg) {
		TaskInfo *info = (TaskInfo *)user_arg;
		info->runnable = true;
	}

};

#endif // __SCHEDULER_HPP_
