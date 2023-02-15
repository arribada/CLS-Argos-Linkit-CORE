#pragma once

#include <stdint.h>
#include "sensor.hpp"

class OEM_RTD_Sensor : public Sensor
{
public:
	OEM_RTD_Sensor();
	double read(unsigned int) override;
	void calibration_write(const double calibration_data, const unsigned int calibration_offset) override;

private:

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
		RTD_READING = 0x0E                   // Read-only
	};

	template <typename T>
	T readReg(RegAddr address);

	template <typename T>
	void writeReg(RegAddr address, T value);
};
