#include "bsp.hpp"
#include "pmu.hpp"
#include "gpio.hpp"
#include "nrf_nvic.h"
#include "nrf_pwr_mgmt.h"

void PMU::initialise() {
	nrf_pwr_mgmt_init();
}

void PMU::reset() {
	sd_nvic_SystemReset();
}

void PMU::powerdown() {
	GPIOPins::clear(BSP::GPIO::GPIO_POWER_CONTROL);
}

void PMU::run() {
	nrf_pwr_mgmt_run();
}
