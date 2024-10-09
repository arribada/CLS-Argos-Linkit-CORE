#include <algorithm>
#include <cmath>

#include "bsp.hpp"
#include "ad5933.hpp"
#include "nrf_i2c.hpp"
#include "error.hpp"
#include "pmu.hpp"
#include "debug.hpp"


AD5933LL::AD5933LL(unsigned int bus, unsigned char addr) {
	DEBUG_TRACE("AD5933LL::AD5933LL(%u, 0x%02x)", bus, (unsigned int)addr);
	m_bus = bus;
	m_addr = addr;
	powerdown();
}

uint8_t AD5933LL::read_reg(const AD5933Register reg)
{
	uint8_t value;

	NrfI2C::write(m_bus, m_addr, (const uint8_t *)&reg, sizeof(reg), false);
	NrfI2C::read(m_bus, m_addr, (uint8_t *)&value, sizeof(value));

    return value;
}

void AD5933LL::write_reg(const AD5933Register reg, uint8_t value)
{
    uint8_t buffer[2];
    buffer[0] = (uint8_t)reg;
    buffer[1] = value;

    NrfI2C::write(m_bus, m_addr, buffer, 2, false);
}

void AD5933LL::start(const unsigned int frequency, const VRange vrange)
{
	DEBUG_TRACE("AD5933LL::start");
	reset();
	gain(vrange);
	set_settling_times(128);
	set_start_frequency(frequency);
	set_frequency_increment(1);
	set_number_of_increments(0);
	standby();
	initialize();
	PMU::delay_ms(10);
	startsweep();
}

void AD5933LL::stop() {
	DEBUG_TRACE("AD5933LL::stop");
	powerdown();
}

double AD5933LL::get_impedence(const unsigned int averaging, const double gain)
{
	DEBUG_TRACE("AD5933LL::get_impedence");
	double impedence = 0;
	for (unsigned int i = 0; i < averaging; i++) {
		wait_iq_data_ready();
		double magnitude = std::sqrt(std::pow((double)read_real(), 2) + std::pow((double)read_imag(), 2));
		impedence += 1 / (magnitude * gain);
	}
	impedence /= averaging;

	DEBUG_TRACE("AD5933LL::get_impedence() = %lf", impedence);

	return impedence;
}

void AD5933LL::get_real_imaginary(int16_t& real, int16_t& imag)
{
	DEBUG_TRACE("AD5933LL::get_real_imaginary");

	wait_iq_data_ready();
	real = read_real();
	imag = read_imag();

	DEBUG_TRACE("AD5933LL::get_real_imaginary() = %d,%d", (int)real, (int)imag);
#ifdef DEBUG_AD5933
	dump_regs();
#endif
}

void AD5933LL::set_start_frequency(unsigned int v)
{
	uint64_t frequency_conversion = (((uint64_t)v << 27) + 2097000) / 4194000; // Assumes 16.776MHz osc
	DEBUG_TRACE("AD5933LL::set_start_frequency: code=%08x", (unsigned int)frequency_conversion);
	write_reg(AD5933Register::START_FREQUENCY_23_16, frequency_conversion >> 16);
	write_reg(AD5933Register::START_FREQUENCY_15_8, frequency_conversion >> 8);
	write_reg(AD5933Register::START_FREQUENCY_7_0, frequency_conversion);
}

void AD5933LL::set_number_of_increments(unsigned int num)
{
	DEBUG_TRACE("AD5933LL::set_number_of_increments");
	write_reg(AD5933Register::NUM_INCREMENTS_15_8, num >> 8);
	write_reg(AD5933Register::NUM_INCREMENTS_7_0, num);
}

void AD5933LL::set_frequency_increment(unsigned int v)
{
	uint64_t frequency_conversion = (((uint64_t)v << 27) + 2097000) / 4194000; // Assumes 16.776MHz osc
	DEBUG_TRACE("AD5933LL::set_frequency_increment: code=%08x", (unsigned int)frequency_conversion);
	write_reg(AD5933Register::FREQUENCY_INCREMENT_23_16, frequency_conversion >> 16);
	write_reg(AD5933Register::FREQUENCY_INCREMENT_15_8, frequency_conversion >> 8);
	write_reg(AD5933Register::FREQUENCY_INCREMENT_7_0, frequency_conversion);
}

void AD5933LL::set_settling_times(unsigned int settling)
{
	DEBUG_TRACE("AD5933LL::set_settling_times");
	uint8_t decode = 0;
	settling = std::min(settling, (unsigned int)2047);

	if (settling > 1023) {
		decode = 3; // 4X
		settling >>= 2;
	} else if (settling > 511) {
		decode = 1; // 2X
		settling >>= 1;
	}

	write_reg(AD5933Register::SETTLING_TIMES_15_8, (settling >> 8) | (decode << 1));
	write_reg(AD5933Register::SETTLING_TIMES_7_0, settling);
}

void AD5933LL::initialize()
{
	DEBUG_TRACE("AD5933LL::initialize");
	write_reg(AD5933Register::CONTROL_HIGH, (uint8_t)AD5933ControlRegisterHigh::INIT_START_FREQUENCY | m_gain_setting);
}

void AD5933LL::reset()
{
	DEBUG_TRACE("AD5933LL::reset");
	write_reg(AD5933Register::CONTROL_LOW, (uint8_t)AD5933ControlRegisterLow::RESET);
}

void AD5933LL::standby()
{
	DEBUG_TRACE("AD5933LL::standby");
	write_reg(AD5933Register::CONTROL_HIGH, (uint8_t)AD5933ControlRegisterHigh::STANDBY | m_gain_setting);
}

void AD5933LL::gain(VRange setting) {
	DEBUG_TRACE("AD5933LL::gain");

	switch (setting) {
	case VRange::V1_GAIN1X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::PGA_GAIN_1X | (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_1V_PP;
		break;
	case VRange::V2_GAIN1X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::PGA_GAIN_1X | (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_2V_PP;
		break;
	case VRange::V200MV_GAIN1X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::PGA_GAIN_1X | (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_200MV_PP;
		break;
	case VRange::V400MV_GAIN1X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::PGA_GAIN_1X | (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_400MV_PP;
		break;
	case VRange::V1_GAIN0_5X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_1V_PP;
		break;
	case VRange::V2_GAIN0_5X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_2V_PP;
		break;
	case VRange::V200MV_GAIN0_5X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_200MV_PP;
		break;
	case VRange::V400MV_GAIN0_5X:
		m_gain_setting = (uint8_t)AD5933ControlRegisterHigh::OPV_RANGE_400MV_PP;
		break;
	default:
		m_gain_setting = 0;
		break;
	}

	write_reg(AD5933Register::CONTROL_HIGH, m_gain_setting);
}

void AD5933LL::startsweep()
{
	DEBUG_TRACE("AD5933LL::startsweep");
	write_reg(AD5933Register::CONTROL_HIGH, (uint8_t)AD5933ControlRegisterHigh::START_FREQUENCY_SWEEP | m_gain_setting);
}

uint8_t AD5933LL::status()
{
	return read_reg(AD5933Register::STATUS);
}

int16_t AD5933LL::read_real() {
	uint8_t low = read_reg(AD5933Register::REAL_7_0);
	uint8_t high = read_reg(AD5933Register::REAL_15_8);
	DEBUG_TRACE("REAL_7_0=%02x REAL_15_8=%02x", (unsigned int)low, (unsigned int)high);
	int x = (int)high << 8 | low;
	return (int16_t)x;
}

int16_t AD5933LL::read_imag() {
	uint8_t low = read_reg(AD5933Register::IMAG_7_0);
	uint8_t high = read_reg(AD5933Register::IMAG_15_8);
	DEBUG_TRACE("IMAG_7_0=%02x IMAG_15_8=%02x", (unsigned int)low, (unsigned int)high);
	int x = (int)high << 8 | low;
	return (int16_t)x;
}

void AD5933LL::wait_iq_data_ready()
{
	DEBUG_TRACE("AD5933LL::wait_iq_data_ready");
	while (0 == (status() & (uint8_t)AD5933StatusRegister::VALID_REAL_IMAG_DATA)) ;
}

void AD5933LL::powerdown()
{
	DEBUG_TRACE("AD5933LL::powerdown");
	write_reg(AD5933Register::CONTROL_HIGH, (uint8_t)AD5933ControlRegisterHigh::POWER_DOWN);
}

void AD5933LL::dump_regs()
{
    uint8_t value;
    for (unsigned int i = 0x80; i < 0x98; i++) {
        value = read_reg((AD5933Register)i);
        DEBUG_TRACE("reg[%02x]=%02x", i, (unsigned int)value);
    }
}