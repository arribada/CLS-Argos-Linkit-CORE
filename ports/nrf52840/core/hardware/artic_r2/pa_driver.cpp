#include "pa_driver.hpp"
#include "error.hpp"
#include "bsp.hpp"
#include "gpio.hpp"
#include "nrfx_twim.h"
#include "debug.hpp"


PADriver::PADriver() {
#if NO_ARGOS_PA_GAIN_CTRL == 1
	m_interface = new DummyPA();
#else
	try {
		DEBUG_TRACE("Checking for MCP4716...");
		m_interface = new MCP47X6();
		DEBUG_TRACE("MCP4716 found");
	} catch (...) {
		DEBUG_TRACE("MCP4716 not found - using RFPA133");
		m_interface = new RFPA133();
	}
#endif
}

PADriver::~PADriver() {
	delete m_interface;
}


void PADriver::set_output_power(unsigned int mW) {
	m_interface->set_output_power(mW);
}

#if NO_ARGOS_PA_GAIN_CTRL == 0

RFPA133::RFPA133() {
}

void RFPA133::set_output_power(unsigned int mW) {
	if (mW <= 1) {
		GPIOPins::clear(BSP::GPIO::GPIO_G8_33);
		GPIOPins::clear(BSP::GPIO::GPIO_G16_33);
	} else if (mW <= 6) {
		GPIOPins::set(BSP::GPIO::GPIO_G8_33);
		GPIOPins::clear(BSP::GPIO::GPIO_G16_33);
	} else if (mW <= 40) {
		GPIOPins::clear(BSP::GPIO::GPIO_G8_33);
		GPIOPins::set(BSP::GPIO::GPIO_G16_33);
	} else {
		GPIOPins::set(BSP::GPIO::GPIO_G8_33);
		GPIOPins::set(BSP::GPIO::GPIO_G16_33);
	}
}

void MCP47X6::set_output_power(unsigned int mW) {
	if (mW == 0) {
		power_down();
	} else if (mW <= 3) {
		set_level(2500);
	} else if (mW <= 5) {
		set_level(2525);
	} else if (mW <= 40) {
		set_level(2555);
	} else if (mW <= 50) {
		set_level(2560);
	} else if (mW <= 200) {
		set_level(2630);
	} else if (mW <= 350) {
		set_level(2674);
	} else if (mW <= 500) {
		set_level(2700);
	} else if (mW <= 750) {
		set_level(2764);
	} else if (mW <= 1000) {
		set_level(2812);
	} else {
		set_level(3090);
	}
}

void MCP47X6::set_vref(uint8_t vref) {
	m_config_reg = (m_config_reg & MCP47X6_VREF_MASK) | (vref & !MCP47X6_VREF_MASK);
}

void MCP47X6::set_gain(uint8_t gain) {
	m_config_reg = (m_config_reg & MCP47X6_GAIN_MASK) | (gain & !MCP47X6_GAIN_MASK);
}

void MCP47X6::set_level(uint16_t level) {
	uint8_t xfer[3];
	DEBUG_TRACE("MCP47X6::set_level: Set DAC level to %hu", level);
	xfer[0] = (m_config_reg | MCP47X6_CMD_VOLALL) & MCP47X6_PWRDN_MASK;
	xfer[1] = (uint8_t)(level >> 4);
	xfer[2] = (uint8_t)(level << 4);
	nrfx_err_t error = nrfx_twim_tx(&BSP::I2C_Inits[MCP4716_DEVICE].twim, MCP4716_I2C_ADDR, xfer, sizeof(xfer), false);
    if (error != NRFX_SUCCESS)
        throw ErrorCode::I2C_COMMS_ERROR;
}

void MCP47X6::power_down() {
	uint8_t xfer[1];
	xfer[0] = m_config_reg | MCP47X6_CMD_VOLCONFIG;
	nrfx_err_t error = nrfx_twim_tx(&BSP::I2C_Inits[MCP4716_DEVICE].twim, MCP4716_I2C_ADDR, xfer, sizeof(xfer), false);
    if (error != NRFX_SUCCESS)
        throw ErrorCode::I2C_COMMS_ERROR;
}

MCP47X6::MCP47X6() {
	m_config_reg = 0;
	set_vref(MCP47X6_VREF_VDD);
	set_gain(MCP47X6_GAIN_1X);
	set_level(0);
}

MCP47X6::~MCP47X6() {
}

#endif
