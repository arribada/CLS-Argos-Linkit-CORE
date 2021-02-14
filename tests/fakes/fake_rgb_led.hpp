#pragma once

#include "rgb_led.hpp"
#include "debug.hpp"

class FakeRGBLed : public RGBLed {
private:
	const char *m_name;
	bool m_is_flashing;
	int m_pin_red;
	int m_pin_green;
	int m_pin_blue;
	RGBLedColor m_color;

public:
	unsigned int m_period;

	FakeRGBLed(const char *name, int red=0, int green=0, int blue=0) {
		m_name = name;
		m_pin_red = red;
		m_pin_green = green;
		m_pin_blue = blue;
		m_period = 0;
		off();
	}
	void set(RGBLedColor color) override {
		m_color = color;
		m_is_flashing = false;
		DEBUG_TRACE("LED[%s]=%s", m_name, color_to_string(color).c_str());
	}
	void off() override {
		set(RGBLedColor::BLACK);
	}
	void flash(RGBLedColor color, unsigned int period_ms = 500) override {
		m_period = period_ms;
		m_color = color;
		m_is_flashing = true;
		DEBUG_TRACE("LED[%s]=flashing %s", m_name, color_to_string(color).c_str());
	}
	bool is_flashing() override {
		return m_is_flashing;
	}
	RGBLedColor get_state() override {
		return m_color;
	}
};

