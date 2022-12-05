#pragma once

#include "switch.hpp"

class FakeSwitch : public Switch {
public:
	FakeSwitch() : Switch(0, 0) {}
	// These methods are test helpers and not part of the Switch interface
	void set_state(bool state) {
		m_current_state = state;
		if (m_state_change_handler && !m_is_paused)
			m_state_change_handler(state);
	}
	bool is_started() { return m_state_change_handler != 0; }
};
