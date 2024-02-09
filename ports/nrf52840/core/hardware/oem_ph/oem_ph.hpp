#pragma once

#include <stdint.h>
#include <optional>
#include "sensor.hpp"

class OEM_PH_Sensor : public Sensor
{
public:
	OEM_PH_Sensor();
	double read(unsigned int) override;
	void calibration_write(const double, const unsigned int) override;

private:

	void set_temperature_if_set();

	std::optional<uint16_t> m_supplied_temperature;
	bool m_first_supplied_temperature;

	enum class RegAddr : uint8_t
	{
		DEVICE_TYPE = 0x00,                  // Read-only
		FIRMWARE_VERSION = 0x01,             // Read-only
		ADDRESS_LOCK_UNLOCK = 0x02,
		ADDRESS = 0x03,
		INTERRUPT_CTRL = 0x04,
		LED_CTRL = 0x05,
		ACTIVE_HIBERNATE = 0x06,
		NEW_READING_AVAILABLE = 0x07,
		CALIBRATION = 0x08,
		CALIBRATION_REQUEST = 0x0C,
		CALIBRATION_CONFIRMATION = 0x0D,     // Read-only
		TEMPERATURE_COMPENSATION = 0x0E,
		TEMPERATURE_CONFIRMATION = 0x12,     // Read-only
		PH_READING = 0x16                    // Read-only
	};

	template <typename T>
	T readReg(RegAddr address);

	template <typename T>
	void writeReg(RegAddr address, T value);
};
