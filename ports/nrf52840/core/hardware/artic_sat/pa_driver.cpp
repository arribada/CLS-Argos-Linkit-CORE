#include "pa_driver.hpp"
#include "error.hpp"
#include "bsp.hpp"
#include "gpio.hpp"
#include "nrf_i2c.hpp"
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

void PADriver::shutdown(void) {
#if NO_ARGOS_PA_GAIN_CTRL == 1
	DummyPA::shutdown();
#else
	try {
		MCP47X6::shutdown();
	} catch (...) {
		RFPA133::shutdown();
	}
#endif
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

void RFPA133::shutdown(void) {
	GPIOPins::clear(BSP::GPIO::GPIO_G8_33);
	GPIOPins::clear(BSP::GPIO::GPIO_G16_33);
}

void MCP47X6::shutdown(void) {
	uint8_t xfer[1];
	DEBUG_TRACE("MCP47X6::power_down: Shutdown DAC");
	xfer[0] = MCP47X6_CMD_VOLCONFIG | MCP47X6_PWRDN_500K;
	NrfI2C::write(MCP4716_DEVICE, MCP4716_I2C_ADDR, xfer, sizeof(xfer), false);
}

void MCP47X6::set_output_power(unsigned int mW) {

	if (mW == 0) {
		power_down();
		return;
	}

	// Try to look up from the calibration file
	try {
		unsigned int dac_value = (unsigned int)m_cal.read(mW);
		DEBUG_TRACE("MCP47X6::set_output_power: using calibration value %u @ %u mW", dac_value, mW);
		set_level(dac_value);
		return;
	} catch (...) {
		DEBUG_TRACE("MCP47X6::set_output_power: missing calibration point @ %u mW", mW);
	}

	// Use defaults where no calibration file is present
	if (mW <= 3) {
		set_level(2569);
	} else if (mW <= 5) {
		set_level(2570);
	} else if (mW <= 40) {
		set_level(2589);
	} else if (mW <= 50) {
		set_level(2595);
	} else if (mW <= 200) {
		set_level(2678);
	} else if (mW <= 350) {
		set_level(2761);
	} else if (mW <= 500) {
		set_level(2844);
	} else if (mW <= 750) {
		set_level(2983);
	} else if (mW <= 1000) {
		set_level(3121);
	} else {
		set_level(3398);
	}
}

void MCP47X6::calibration_write(const double value, const unsigned int offset) {
	if (offset == 0) { // 0=>reset
		m_cal.reset();
	} else if (offset == 1) { // 1=>save
		m_cal.save();
	} else { // DAC value for given power in mW
		m_cal.write(offset, value);
	}
}

void MCP47X6::calibration_save(bool force) {
	m_cal.save(force);
}

void MCP47X6::set_vref(uint8_t vref) {
	m_config_reg = (m_config_reg & MCP47X6_VREF_MASK) | (vref & ~MCP47X6_VREF_MASK);
}

void MCP47X6::set_gain(uint8_t gain) {
	m_config_reg = (m_config_reg & MCP47X6_GAIN_MASK) | (gain & ~MCP47X6_GAIN_MASK);
}

void MCP47X6::set_level(uint16_t level) {
	uint8_t xfer[3];
	DEBUG_TRACE("MCP47X6::set_level: Set DAC level to %hu", level);
	xfer[0] = (m_config_reg | MCP47X6_CMD_VOLALL) & MCP47X6_PWRDN_MASK;
	xfer[1] = (uint8_t)(level >> 4);
	xfer[2] = (uint8_t)(level << 4);
	NrfI2C::write(MCP4716_DEVICE, MCP4716_I2C_ADDR, xfer, sizeof(xfer), false);
}

void MCP47X6::power_down() {
	uint8_t xfer[1];
	DEBUG_TRACE("MCP47X6::power_down: Shutdown DAC");
	xfer[0] = m_config_reg | MCP47X6_CMD_VOLCONFIG | MCP47X6_PWRDN_500K;
	NrfI2C::write(MCP4716_DEVICE, MCP4716_I2C_ADDR, xfer, sizeof(xfer), false);
}

MCP47X6::MCP47X6() : Calibratable("MCP47X6"), m_cal(Calibration("MCP47X6")), m_config_reg(0) {
	set_vref(MCP47X6_VREF_VREFPIN);
	set_gain(MCP47X6_GAIN_1X);
	set_output_power(0);
}

MCP47X6::~MCP47X6() {
}

#endif
