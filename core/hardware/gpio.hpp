#ifndef __GPIO_HPP_
#define __GPIO_HPP_

#include <stdint.h>
#include "bsp.hpp"

class GPIOPins {
public:
	static void initialise();
	static void set(BSP::GPIO pin);
	static void clear(BSP::GPIO pin);
	static uint32_t value(BSP::GPIO pin);
};

#endif // __GPIO_HPP_
