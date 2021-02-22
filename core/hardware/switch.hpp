#ifndef __SWITCH_HPP_
#define __SWITCH_HPP_

#include <functional>

using namespace std;


class Switch {
protected:
	std::function<void(bool)> m_state_change_handler;
	int m_pin;
	unsigned int m_hysteresis_time_ms;
	bool m_current_state;

public:
	Switch(int pin, unsigned int hysteresis_time_ms) {
		m_pin = pin;
		m_hysteresis_time_ms = hysteresis_time_ms;
		m_current_state = false;
	}
	virtual ~Switch() {
	}
	bool get_state() {
		return m_current_state;
	}
	virtual void start(std::function<void(bool)> func) {
		m_state_change_handler = func;
	}
	virtual void stop() {
		m_state_change_handler = nullptr;
	}
};

#endif // __SWITCH_HPP_
