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

EZO_RTD_Sensor::EZO_RTD_Sensor() : Sensor("RTD"), m_is_calibrating(false) {
	// Wakeup the device from sleep mode to confirm it is present
	wakeup();

	// Sleep the device once more
	sleep();
}

void EZO_RTD_Sensor::sleep() {
	write_command("Sleep");
}

void EZO_RTD_Sensor::wakeup() {
	read_response();
	read_response();
}

double EZO_RTD_Sensor::read(unsigned int)
{
	char response[20];

	// Wakeup the device from sleep mode
	wakeup();

	// Issue read command
	write_command("R");
	PMU::delay_ms(600);
	wait_response(response);

	if (!m_is_calibrating)
		sleep();

	// Response is a string containing floating point temperature
    char *e;
    errno = 0;
    double value = strtod(response, &e);

    // Check reading is within the valid range of the device i.e., -126C to 1254C
    if (errno == 0 && response != e && value >= -126 && value <= 1254) {
    	return value;
    }
  	throw ErrorCode::I2C_COMMS_ERROR;
}

void EZO_RTD_Sensor::calibration_write(const double temperature, const unsigned int calibration_offset)
{
	// Wakeup the device in case it is in sleep mode
	wakeup();

	m_is_calibrating = true;

	if (calibration_offset == 0) {
		write_command("Cal,clear");
		PMU::delay_ms(300);
	} else if (calibration_offset == 1) {
		write_command("Cal,0");
		PMU::delay_ms(600);
	} else if (calibration_offset == 2) {
		write_command("Cal,100");
		PMU::delay_ms(600);
	} else if (calibration_offset == 3) {
		if (temperature >= -126 && temperature <= 1254) {
			char command[255];
			sprintf(command, "Cal,%f", temperature);
			write_command(command);
			PMU::delay_ms(600);
		} else
			throw ErrorCode::RESOURCE_NOT_AVAILABLE;
	} else if (calibration_offset == 4) {
		write_command("Find");
		PMU::delay_ms(300);
	} else if (calibration_offset == 5) {
		write_command("Factory");
		m_is_calibrating = false;
		return;
	} else if (calibration_offset >= 6) {
		sleep();
		m_is_calibrating = false;
		return;
	}

	wait_response();
}

void EZO_RTD_Sensor::write_command(const char *command) {
	char data[255];
	strcpy(data, command); // I2C driver requires data to be in RAM region!
	DEBUG_TRACE("EZO_RTD_Sensor::write_command(%s)", command);
	NrfI2C::write(EZO_RTD_DEVICE, EZO_RTD_DEVICE_ADDR, (const uint8_t *)data, strlen(command), false);
}

const char *EZO_RTD_Sensor::response_code_to_str(ResponseCode resp) {
	switch (resp) {
	case ResponseCode::SUCCESS:
		return "SUCCESS";
	case ResponseCode::ERROR:
		return "ERROR";
	case ResponseCode::BUSY:
		return "BUSY";
	case ResponseCode::NODATA:
		return "NODATA";
	case ResponseCode::UNKNOWN:
	default:
		return "UNKNOWN";
	}
}

EZO_RTD_Sensor::ResponseCode EZO_RTD_Sensor::read_response(char *response) {
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
		resp = ResponseCode::UNKNOWN;

	if (resp == ResponseCode::SUCCESS && response != nullptr) {
		unsigned int i;
		for (i = 1; i < sizeof(bytes) && bytes[i]; i++)
			response[i-1] = (char)bytes[i];
		response[i-1] = 0; // Force null termination
		DEBUG_TRACE("EZO_RTD_Sensor::read_response: resp=%s data=%s", response_code_to_str(resp), response);
	} else {
		DEBUG_TRACE("EZO_RTD_Sensor::read_response: resp=%s", response_code_to_str(resp));
	}

	return resp;
}

void EZO_RTD_Sensor::wait_response(char *response) {
	int retries;
	for (retries = 0; retries < 100; retries++) {
		if (read_response(response) == ResponseCode::SUCCESS)
			break;
		PMU::delay_ms(1);
	}

	if (retries == 100)
		throw ErrorCode::I2C_COMMS_ERROR;
}
