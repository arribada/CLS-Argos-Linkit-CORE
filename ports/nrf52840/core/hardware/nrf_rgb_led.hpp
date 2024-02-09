#pragma once

#include <stdint.h>

#include "timer.hpp"
#include "rgb_led.hpp"
#include "debug.hpp"
#include "gpio.hpp"
#include "interrupt_lock.hpp"

extern Timer *system_timer;


class NrfRGBLed : public RGBLed {
private:
	int m_pin_red;
	int m_pin_green;
	int m_pin_blue;
	RGBLedColor m_color;
	RGBLedColor m_flash_color;
	bool m_is_flashing;
	bool m_flash_state;
	unsigned int m_flash_interval;
	const char *m_name;
	Timer::TimerHandle m_led_timer;

	void toggle_led(void) {
		InterruptLock lock;
		if (!m_is_flashing)
			return;
		if (m_flash_state)
			set_color(m_flash_color);
		else
			set_color(RGBLedColor::BLACK);
		m_flash_state = !m_flash_state;
		m_led_timer = system_timer->add_schedule([this]() {
			if (m_is_flashing)
				toggle_led();
		}, system_timer->get_counter() + m_flash_interval);
	}

private:
	void set_color(RGBLedColor color) {
		switch (color) {
		case RGBLedColor::BLACK:
			GPIOPins::set(m_pin_red);
			GPIOPins::set(m_pin_green);
			GPIOPins::set(m_pin_blue);
			break;
		case RGBLedColor::RED:
			GPIOPins::clear(m_pin_red);
			GPIOPins::set(m_pin_green);
			GPIOPins::set(m_pin_blue);
			break;
		case RGBLedColor::GREEN:
			GPIOPins::set(m_pin_red);
			GPIOPins::clear(m_pin_green);
			GPIOPins::set(m_pin_blue);
			break;
		case RGBLedColor::BLUE:
			GPIOPins::set(m_pin_red);
			GPIOPins::set(m_pin_green);
			GPIOPins::clear(m_pin_blue);
			break;
		case RGBLedColor::CYAN:
			GPIOPins::set(m_pin_red);
			GPIOPins::clear(m_pin_green);
			GPIOPins::clear(m_pin_blue);
			break;
		case RGBLedColor::MAGENTA:
			GPIOPins::clear(m_pin_red);
			GPIOPins::set(m_pin_green);
			GPIOPins::clear(m_pin_blue);
			break;
		case RGBLedColor::YELLOW:
			GPIOPins::clear(m_pin_red);
			GPIOPins::clear(m_pin_green);
			GPIOPins::set(m_pin_blue);
			break;
		case RGBLedColor::WHITE:
			GPIOPins::clear(m_pin_red);
			GPIOPins::clear(m_pin_green);
			GPIOPins::clear(m_pin_blue);
			break;
		default:
			break;
		}
	}
public:
	NrfRGBLed(const char *name, int red, int green, int blue, RGBLedColor color = RGBLedColor::BLACK) {
		m_name = name;
		m_pin_red = red;
		m_pin_green = green;
		m_pin_blue = blue;
		m_color = color;
		set(color);
	}
	void set(RGBLedColor color) override {
		InterruptLock lock;
		system_timer->cancel_schedule(m_led_timer);
		m_is_flashing = false;
		m_color = color;
		set_color(color);
		//DEBUG_TRACE("LED[%s]=%s", m_name, color_to_string(color).c_str());
	}
	void off() override {
		set(RGBLedColor::BLACK);
	}
	void flash(RGBLedColor color, unsigned int interval_ms = 500) override {
		InterruptLock lock;
		system_timer->cancel_schedule(m_led_timer);
		m_flash_interval = interval_ms;
		m_flash_color = color;
		m_is_flashing = true;
		m_flash_state = true;
		toggle_led();
		//DEBUG_TRACE("LED[%s]=flashing %s", m_name, color_to_string(color).c_str());
	}
	bool is_flashing() override {
		return m_is_flashing;
	}
	RGBLedColor get_state() override {
		return m_color;
	}
};
