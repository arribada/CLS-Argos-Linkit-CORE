#include <stdint.h>

#include "gpio.hpp"
#include "bsp.hpp"

void GPIOPins::initialise()
{
	for (uint32_t i = 0; i < (unsigned int)BSP::GPIO::GPIO_TOTAL_NUMBER; i++)
	{
		nrf_gpio_cfg(i,
					 BSP::GPIO_Inits[i].dir, BSP::GPIO_Inits[i].input, BSP::GPIO_Inits[i].pull,
				     BSP::GPIO_Inits[i].drive, BSP::GPIO_Inits[i].sense);
	}
}

void GPIOPins::set(BSP::GPIO pin)
{
	nrf_gpio_pin_set((uint32_t)pin);
}

void GPIOPins::clear(BSP::GPIO pin)
{
	nrf_gpio_pin_clear((uint32_t)pin);
}

uint32_t GPIOPins::value(BSP::GPIO pin)
{
	return nrf_gpio_pin_read((uint32_t)pin);
}
