#pragma once

#include <cstdint>
#include "sensor.hpp"
#include "bsp.hpp"
#include "error.hpp"

class MS5803LL {
public:
	MS5803LL(unsigned int bus, unsigned char address);
	void read(double& temperature, double& pressure);

private:
	unsigned int m_bus;
	unsigned char m_addr;
	enum MS5803Command : uint8_t {
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
	};
	uint16_t m_coefficients[8];

	void send_command(uint8_t command);
	void read_coeffs();
	uint32_t sample_adc(uint8_t measurement);
	void check_coeffs();
};


class MS5803 : public Sensor {
public:
	MS5803() : Sensor("PRS"), m_ms5803(MS5803LL(MS5803_DEVICE, MS5803_ADDRESS)) {}
	void calibrate(double, unsigned int) override {};
	double read(unsigned int channel = 0) {
		if (0 == channel) {
			m_ms5803.read(m_last_temperature, m_last_pressure);
			return m_last_pressure;
		} else if (1 == channel) {
			return m_last_temperature;
		}
		throw ErrorCode::BAD_SENSOR_CHANNEL;
	}

private:
	double m_last_pressure;
	double m_last_temperature;

	MS5803LL m_ms5803;
};
