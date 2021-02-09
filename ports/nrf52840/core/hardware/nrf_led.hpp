#ifndef __NRF_LED_HPP_
#define __NRF_LED_HPP_

#include <stdint.h>
#include <functional>

#include "scheduler.hpp"
#include "led.hpp"
#include "debug.hpp"
#include "gpio.hpp"


extern Scheduler *system_scheduler;


class NrfLed : public Led {
private:
	uint32_t m_pin;
	bool m_is_flashing;
	const char *m_name;
	Scheduler::TaskHandle m_led_task;

	void toggle_led(void) {
		GPIOPins::toggle(m_pin);
		m_led_task = system_scheduler->post_task_prio(std::bind(&NrfLed::toggle_led, this),
				Scheduler::DEFAULT_PRIORITY, 500);
	}

public:
	NrfLed(const char *name, int pin=0) : Led(pin) {
		m_name = name;
		m_pin = (uint32_t)pin;
	}
	bool get_state() { return GPIOPins::value(m_pin); }
	void on() {
		DEBUG_TRACE("LED[%s]=on", m_name);
		system_scheduler->cancel_task(m_led_task);
		GPIOPins::set(m_pin);
	}
	void off() {
		DEBUG_TRACE("LED[%s]=off", m_name);
		system_scheduler->cancel_task(m_led_task);
		GPIOPins::clear(m_pin);
	}
	void flash() {
		DEBUG_TRACE("LED[%s]=flashing", m_name);
		system_scheduler->cancel_task(m_led_task);
		toggle_led();
	}
};

#endif // __NRF_LED_HPP_
