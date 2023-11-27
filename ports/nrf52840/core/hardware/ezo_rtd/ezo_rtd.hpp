#pragma once

#include <stdint.h>
#include "sensor.hpp"

class EZO_RTD_Sensor : public Sensor
{
public:
	enum class ResponseCode {
		SUCCESS,
		ERROR,
		BUSY,
		NODATA
	};
	EZO_RTD_Sensor();
	double read(unsigned int) override;
	void calibration_write(const double calibration_data, const unsigned int calibration_offset) override;

private:
	void write_command(const std::string command);
	ResponseCode read_response(std::string *response = nullptr);
};
