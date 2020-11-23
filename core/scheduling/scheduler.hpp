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

	void run(void (*exception_handler)(int));
	void register_task(Task);
	void post_task_prio(Task, int prio=Scheduler::DEFAULT_PRIORITY, unsigned int delay_ms=0);
	void cancel_task(Task);
	bool is_scheduled(Task);

private:
	std::vector<Task> m_task_queue[Scheduler::MIN_PRIORITY - Scheduler::MAX_PRIORITY + 1];
	void (*m_exception_handler)(int);

	Task pop_task(int prio) {
		Task t = m_task_queue[prio].front();
		m_task_queue[prio].erase(m_task_queue[prio].begin());
		return t;
	}
	bool task_waiting(int prio) {
		return m_task_queue[prio].empty();
	}
	void timer_event();

};

#endif // __SCHEDULER_HPP_
