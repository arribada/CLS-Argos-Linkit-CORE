#ifndef __SCHEDULER_HPP_
#define __SCHEDULER_HPP_

#include <vector>

using namespace std;

using Task = void (*)();

class Scheduler {
public:
	static const int MIN_PRIORITY = 7;
	static const int MAX_PRIORITY = 0;
	static const int DEFAULT_PRIORITY = MIN_PRIORITY;

	static void run(void (*exception_handler)(int));
	static void register_task(Task);
	static void post_task_prio(Task, int prio=Scheduler::DEFAULT_PRIORITY, unsigned int delay_ms=0);
	static void cancel_task(Task);
	static bool is_scheduled(Task);

private:
	static std::vector<Task> m_task_queue[Scheduler::MIN_PRIORITY - Scheduler::MAX_PRIORITY + 1];
	static void (*m_exception_handler)(int);

	static Task pop_task(int prio) {
		Task t = m_task_queue[prio].front();
		m_task_queue[prio].erase(m_task_queue[prio].begin());
		return t;
	}
	static bool task_waiting(int prio) {
		return m_task_queue[prio].empty();
	}
	static void timer_event();

};

#endif // __SCHEDULER_HPP_
