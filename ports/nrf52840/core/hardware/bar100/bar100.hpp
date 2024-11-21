#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "pressure_sensor.hpp"
#include "bsp.hpp"
#include "error.hpp"


class Bar100 : public PressureSensorDevice {
public:
	Bar100(unsigned int bus, unsigned char address);
	void read(double& temperature, double& pressure) override;

private:
	unsigned int m_bus;
	unsigned char m_addr;
	double        m_pmin;
	double        m_pmax;
	double        m_mode_offset;

	void command(uint8_t reg, uint8_t *read_buffer = nullptr, unsigned int length = 0, unsigned int delay_ms = 0);
};
