#include "bar100.hpp"

#include "bsp.hpp"
#include "nrf_i2c.hpp"
#include "error.hpp"
#include "pmu.hpp"
#include "debug.hpp"

// Device register addresses
static constexpr uint8_t CUST_ID0 = 0x00;
static constexpr uint8_t CUST_ID1 = 0x01;
static constexpr uint8_t SCALING0 = 0x12;
static constexpr uint8_t SCALING1 = 0x13;
static constexpr uint8_t SCALING2 = 0x14;
static constexpr uint8_t SCALING3 = 0x15;
static constexpr uint8_t SCALING4 = 0x16;
static constexpr uint8_t REQUEST  = 0xAC;
static constexpr uint8_t BUSY     = 0x20;


void Bar100::command(uint8_t reg, uint8_t *read_buffer, unsigned int length, unsigned int delay_ms) {
    NrfI2C::write(m_bus, m_addr, &reg, 1, false);

    if (length > 0) {
		PMU::delay_ms(delay_ms);
		NrfI2C::read(m_bus, m_addr, read_buffer, length);
    }
}

Bar100::Bar100(unsigned int bus, unsigned char addr) {
	DEBUG_TRACE("Bar100::Bar100(i2cbus=%u, i2caddr=0x%02x)", bus, (unsigned int)addr);
	m_bus = bus;
	m_addr = addr;

	uint8_t read_buffer[3];

	command(CUST_ID0, read_buffer, sizeof(read_buffer), 1);
	uint16_t custid0 = read_buffer[2] | (uint16_t)read_buffer[1] << 8;

	DEBUG_TRACE("Bar100::Bar100: CUST_ID0=%04X", (unsigned int)custid0);

	command(SCALING0, read_buffer, sizeof(read_buffer), 1);
	uint8_t mode = read_buffer[2] & 0x3;

	DEBUG_TRACE("Bar100::Bar100: PA_MODE=%u", (unsigned int)mode);

	if (mode == 0) {
		// PA mode, Vented Gauge. Zero at atmospheric pressure
		m_mode_offset = 1.01325;
	} else if (mode == 1) {
		// PR mode, Sealed Gauge. Zero at 1.0 bar
		m_mode_offset = 1.0;
	} else {
		// PAA mode, Absolute. Zero at vacuum
		// (or undefined mode)
		m_mode_offset = 0;
	}

	union __attribute__((packed)) {
		float value;
		uint32_t value32;
	} pmin, pmax;

	command(SCALING1, read_buffer, sizeof(read_buffer), 1);
	pmin.value32 = (uint32_t)read_buffer[2] << 16 | (uint32_t)read_buffer[1] << 24;

	command(SCALING2, read_buffer, sizeof(read_buffer), 1);
	pmin.value32 |= (uint32_t)read_buffer[2] << 0 | (uint32_t)read_buffer[1] << 8;

	command(SCALING3, read_buffer, sizeof(read_buffer), 1);
	pmax.value32 = (uint32_t)read_buffer[2] << 16 | (uint32_t)read_buffer[1] << 24;

	command(SCALING4, read_buffer, sizeof(read_buffer), 1);
	pmax.value32 |= (uint32_t)read_buffer[2] << 0 | (uint32_t)read_buffer[1] << 8;

	m_pmin = pmin.value;
	m_pmax = pmax.value;

	DEBUG_TRACE("Bar100::Bar100: pmin=%f (%08x) pmax=%f (%08x)", m_pmin,
			(unsigned int)pmin.value32,
			m_pmax,
			(unsigned int)pmax.value32);
}

void Bar100::read(double& temperature, double& pressure) {
	uint8_t read_buffer[5];
	uint16_t pressure16, temperature16;

	// Request measurements and wait 10 ms for the response
	command(REQUEST);

	unsigned int timeout = 10;
	do {
		PMU::delay_ms(1);
		NrfI2C::read(m_bus, m_addr, read_buffer, 1);
	} while ((read_buffer[0] & BUSY) && --timeout > 0);

	NrfI2C::read(m_bus, m_addr, read_buffer, sizeof(read_buffer));

	if (read_buffer[0] & BUSY)
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;

	// Byte swap as sent MSB first
	pressure16 = (uint16_t)read_buffer[1] << 8 | read_buffer[2];
	temperature16 = (uint16_t)read_buffer[3] << 8 | read_buffer[4];

	temperature = (((temperature16 / 16) - 24) * 0.05) - 50;
	pressure = (pressure16 - 16384) * ((m_pmax - m_pmin) / 32768) + m_pmin + m_mode_offset;

	DEBUG_TRACE("Bar100::read: status=%02x pressure16=%04x temp16=%04x p=%f t=%f", (unsigned int)read_buffer[0],
			(unsigned int)pressure16, (unsigned int)temperature16, pressure, temperature);
}
