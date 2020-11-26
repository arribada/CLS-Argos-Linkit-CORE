#ifndef __LEDS_HPP_
#define __LEDS_HPP_

class Led {
public:
	Led(int pin) {};
	virtual void on() = 0;
	virtual void off() = 0;
	virtual bool get_state() = 0;
};

#endif // __LEDS_HPP_
