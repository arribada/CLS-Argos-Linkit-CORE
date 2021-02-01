#ifndef __GPIO_HPP_
#define __GPIO_HPP_

#include <stdint.h>

class GPIOPins {
public:
	static void initialise();
	static void set(uint32_t pin);
	static void clear(uint32_t pin);
	static void toggle(uint32_t pin);
	static uint32_t value(uint32_t pin);
};

#endif // __GPIO_HPP_
