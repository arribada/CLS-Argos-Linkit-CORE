#ifndef __FAKE_LED_HPP_
#define __FAKE_LED_HPP_

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
	bool get_state() { return m_pin_state; }
	void on() {
		DEBUG_TRACE("LED[%s]=on", m_name);
		m_pin_state = true;
		m_is_flashing = false;
	}
	void off() {
		DEBUG_TRACE("LED[%s]=off", m_name);
		m_pin_state = false;
		m_is_flashing = false;
	}
	void flash() {
		DEBUG_TRACE("LED[%s]=flashing", m_name);
		m_is_flashing = true;
	}

	// Fake test helper function
	bool is_flashing() {
		return m_is_flashing;
	}
};

#endif // __FAKE_LED_HPP_
