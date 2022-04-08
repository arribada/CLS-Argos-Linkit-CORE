#include <cstdint>

#include "pressure_sensor.hpp"

class MS5803 : public PressureSensor {
public:
	MS5803();
	double read();

private:
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

	bool detector_state() override;
	void send_command(uint8_t command);
	void read_coeffs();
	uint32_t sample_adc(uint8_t measurement);
	void check_coeffs();
};
