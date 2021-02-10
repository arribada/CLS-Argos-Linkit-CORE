#include "bsp.hpp"
#include "pmu.hpp"
#include "gpio.hpp"
#include "nrf_nvic.h"

void PMU::reset() {
	sd_nvic_SystemReset();
}

void PMU::powerdown() {
	GPIOPins::clear(BSP::GPIO::GPIO_POWER_CONTROL);
}
