#pragma once

#include "gpio.hpp"
#include "led.hpp"
#include "timer.hpp"


extern Timer *system_timer;


class GPIOLed : public Led {
public:
	GPIOLed(int pin) : Led(pin) {
		m_pin = pin;
		off();
	}

	~GPIOLed() {
		off();
	}

private:
	int m_pin;
	bool m_is_flashing;
	bool m_flash_state;
	unsigned int m_flash_interval;
	Timer::TimerHandle m_timer_task;

	void toggle_led(void) {
		InterruptLock lock;
		if (!m_is_flashing)
			return;
		if (m_flash_state)
			GPIOPins::set(m_pin);
		else
			GPIOPins::clear(m_pin);
		m_flash_state = !m_flash_state;
		m_timer_task = system_timer->add_schedule([this]() {
			if (m_is_flashing)
				toggle_led();
		}, system_timer->get_counter() + m_flash_interval);
	}

	void off() {
		m_is_flashing = false;
		system_timer->cancel_schedule(m_timer_task);
		GPIOPins::clear(m_pin);
	}
	void on() {
		m_is_flashing = false;
		system_timer->cancel_schedule(m_timer_task);
		GPIOPins::set(m_pin);
	}
	bool get_state() {
		return m_is_flashing || GPIOPins::value(m_pin);
	}
	void flash(unsigned int interval_ms) {
		InterruptLock lock;
		system_timer->cancel_schedule(m_timer_task);
		m_flash_interval = interval_ms;
		m_is_flashing = true;
		m_flash_state = true;
		toggle_led();
	}
};
