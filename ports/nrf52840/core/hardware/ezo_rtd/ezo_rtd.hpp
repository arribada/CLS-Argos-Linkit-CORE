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
		NODATA,
		UNKNOWN
	};
	EZO_RTD_Sensor();
	double read(unsigned int) override;
	void calibration_write(const double calibration_data, const unsigned int calibration_offset) override;

private:
	bool m_is_calibrating;
	void wakeup();
	void sleep();
	void write_command(const char *command);
	ResponseCode read_response(char *response = nullptr);
	void wait_response(char *response = nullptr);
	static const char *response_code_to_str(ResponseCode resp);
};
