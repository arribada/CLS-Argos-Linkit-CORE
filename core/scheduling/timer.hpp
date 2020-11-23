#ifndef __TIMER_HPP_
#define __TIMER_HPP_

#include <vector>
#include <algorithm>

using namespace std;

using TimerEvent = void(*)();

class Timer {
public:
	unsigned int get_counter();
	void add_event(TimerEvent e) {
		m_events.insert(e);
	}
	void remove_event(TimerEvent e) {
		m_events.erase(std::remove(m_events.begin(), m_events.end(), e), m_events.end());
	}
private:
	std::vector<TimerEvent> m_events;
	void timer_fired();
};

#endif // __TIMER_HPP_
