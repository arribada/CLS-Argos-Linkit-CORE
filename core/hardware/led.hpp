#pragma once

class Led {
public:
	Led(int pin) { (void)pin; };
	virtual ~Led() {}
	virtual void on() = 0;
	virtual void off() = 0;
	virtual bool get_state() = 0;
	virtual void flash(unsigned int period_ms) = 0;
	virtual bool is_flashing() = 0;
};
