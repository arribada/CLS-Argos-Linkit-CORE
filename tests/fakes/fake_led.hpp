#pragma once

#include "led.hpp"
#include "debug.hpp"


class FakeLed : public Led {
private:
	bool m_is_flashing;
	bool m_pin_state;
	const char *m_name;

public:
	FakeLed(const char *name, int pin=0) : Led(pin) {
		m_pin_state = false;
		m_is_flashing = false;
		m_name = name;
	}
	bool get_state() override { return m_pin_state || m_is_flashing; }
	void on() override {
		DEBUG_TRACE("LED[%s]=on", m_name);
		m_pin_state = true;
		m_is_flashing = false;
	}
	void off() override {
		DEBUG_TRACE("LED[%s]=off", m_name);
		m_pin_state = false;
		m_is_flashing = false;
	}
	void flash(unsigned int period) override {
		(void)period;
		DEBUG_TRACE("LED[%s]=flashing %u", m_name, period);
		m_is_flashing = true;
	}

	// Fake test helper function
	bool is_flashing() override {
		return m_is_flashing;
	}
};
