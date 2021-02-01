#include <stdint.h>

#include "gpio.hpp"
#include "bsp.hpp"

void GPIOPins::initialise()
{
	for (uint32_t i = 0; i < (uint32_t)BSP::GPIO::GPIO_TOTAL_NUMBER; i++)
	{
		nrf_gpio_cfg(i,
					 BSP::GPIO_Inits[i].dir, BSP::GPIO_Inits[i].input, BSP::GPIO_Inits[i].pull,
				     BSP::GPIO_Inits[i].drive, BSP::GPIO_Inits[i].sense);
	}
}

void GPIOPins::set(uint32_t pin)
{
	nrf_gpio_pin_set(pin);
}

void GPIOPins::clear(uint32_t pin)
{
	nrf_gpio_pin_clear(pin);
}

void GPIOPins::toggle(uint32_t pin)
{
	nrf_gpio_pin_toggle(pin);
}

uint32_t GPIOPins::value(uint32_t pin)
{
	return nrf_gpio_pin_read(pin);
}
