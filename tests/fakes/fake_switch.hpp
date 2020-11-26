#ifndef __FAKE_SWITCH_HPP_
#define __FAKE_SWITCH_HPP_

#include "switch.hpp"

class FakeSwitch : public Switch {
public:
	FakeSwitch(int pin, unsigned int hysteresis_time_ms) : Switch(pin, hysteresis_time_ms) {}
	// These methods are test helpers and not part of the Switch interface
	void set_state(bool state) {
		m_current_state = state;
		if (m_state_change_handler)
			m_state_change_handler(state);
	}
	bool is_started() { return m_state_change_handler != 0; }
};

#endif // __FAKE_LED_HPP_
