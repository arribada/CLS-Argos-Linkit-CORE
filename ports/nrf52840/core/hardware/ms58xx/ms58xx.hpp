#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "pressure_sensor.hpp"
#include "bsp.hpp"
#include "error.hpp"

class MS58xxLL : public PressureSensorDevice {
public:
	MS58xxLL(unsigned int bus, unsigned char address, std::string variant);
	void read(double& temperature, double& pressure) override;

private:
	unsigned int m_bus;
	unsigned char m_addr;
	enum MS58xxCommand : uint8_t {
		RESET       = (0x1E), // ADC reset command
		PROM        = (0xA0), // PROM location
		ADC_READ    = (0x00), // ADC read command
		ADC_CONV    = (0x40), // ADC conversion command
		ADC_D1      = (0x00), // ADC D1 conversion
		ADC_D2      = (0x10), // ADC D2 conversion
		ADC_256     = (0x00), // ADC resolution=256
		ADC_512     = (0x02), // ADC resolution=512
		ADC_1024    = (0x04), // ADC resolution=1024
		ADC_2048    = (0x06), // ADC resolution=2048
		ADC_4096    = (0x08), // ADC resolution=4096
		ADC_8192    = (0x0A), // ADC resolution=8192
	};
	uint16_t C[8];
	std::function<void(const int32_t, const int32_t, const uint16_t *, double&, double&)>  m_convert;
	MS58xxCommand m_resolution;
	unsigned int m_prom_size;
	unsigned int m_crc_offset;
	unsigned int m_crc_mask;
	unsigned int m_crc_shift;

	void send_command(uint8_t command);
	void read_coeffs();
	uint32_t sample_adc(uint8_t measurement);
	void check_coeffs();
};

