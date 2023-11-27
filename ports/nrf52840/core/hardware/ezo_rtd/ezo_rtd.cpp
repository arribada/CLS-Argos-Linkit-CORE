#include <array>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include "nrf_i2c.hpp"
#include "bsp.hpp"
#include "error.hpp"
#include "gpio.hpp"
#include "debug.hpp"
#include "ezo_rtd.hpp"
#include "pmu.hpp"

EZO_RTD_Sensor::EZO_RTD_Sensor() : Sensor("RTD") {
	read_response(); // Should get a NODATA response code

#ifdef EZO_RTD_LED_OFF
	// Turn off LED
	write_command("L,0"); // Turn LED off
	PMU::delay_ms(300);
	int retries;
	for (retries = 0; retries < 10; retries++) {
		if (read_response() == ResponseCode::SUCCESS)
			break;
	}
	if (retries == 10)
		throw ErrorCode::I2C_COMMS_ERROR;
#endif
}

double EZO_RTD_Sensor::read(unsigned int)
{
	std::string response;

	write_command("R");
	PMU::delay_ms(600);

	int retries;
	for (retries = 0; retries < 10; retries++) {
		if (read_response(&response) == ResponseCode::SUCCESS)
			break;
	}

	if (retries == 10)
		throw ErrorCode::I2C_COMMS_ERROR;

	// Response is a string containing floating point temperature
    const char *s = response.c_str();
    char *e;
    errno = 0;
    double value = strtod(s, &e);
    if (errno == 0 && s != e && value >= -126 && value <= 1254) {
    	return value;
    }
  	throw ErrorCode::I2C_COMMS_ERROR;
}

void EZO_RTD_Sensor::calibration_write(const double, const unsigned int calibration_offset)
{
	if (calibration_offset == 0) {
		write_command("Cal,clear");
		PMU::delay_ms(300);
	} else if (calibration_offset == 1) {
		write_command("Cal,0");
		PMU::delay_ms(600);
	} else if (calibration_offset == 2) {
		write_command("Cal,100");
		PMU::delay_ms(600);
	} else {
		return;
	}

	int retries;
	for (retries = 0; retries < 10; retries++) {
		if (read_response() == ResponseCode::SUCCESS)
			break;
	}

	if (retries == 10)
		throw ErrorCode::I2C_COMMS_ERROR;
}

void EZO_RTD_Sensor::write_command(const std::string command) {
	NrfI2C::write(EZO_RTD_DEVICE, EZO_RTD_DEVICE_ADDR, (const uint8_t *)command.c_str(), command.length(), false);
}

EZO_RTD_Sensor::ResponseCode EZO_RTD_Sensor::read_response(std::string *response) {
	ResponseCode resp;
	uint8_t bytes[20] = {0};

	NrfI2C::read(EZO_RTD_DEVICE, EZO_RTD_DEVICE_ADDR, bytes, sizeof(bytes));
	if (bytes[0] == 0x01)
		resp = ResponseCode::SUCCESS;
	else if (bytes[0] == 0x02)
		resp = ResponseCode::ERROR;
	else if (bytes[0] == 0xFE)
		resp = ResponseCode::BUSY;
	else if (bytes[0] == 0xFF)
		resp = ResponseCode::NODATA;
	else
		throw ErrorCode::I2C_COMMS_ERROR;

	if (resp == ResponseCode::SUCCESS && response != nullptr) {
		for (unsigned int i = 1; i < sizeof(bytes); i++) {
			if (bytes[i] == 0)
				break;
			response->push_back((char)bytes[i]);
		}
	}

	DEBUG_TRACE("EZO_RTD_Sensor::read_response: resp=%u data=%s", (unsigned int)resp,
			response == nullptr ? "null" : response->c_str());

	return resp;
}
