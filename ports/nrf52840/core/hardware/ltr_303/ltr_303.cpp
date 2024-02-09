#include <cstddef>
#include <array>
#include <map>
#include <cstdint>

#include "ltr_303_defs.h"
#include "ltr_303.hpp"
#include "bsp.hpp"
#include "error.hpp"
#include "gpio.hpp"
#include "nrf_delay.h"
#include "nrf_i2c.hpp"
#include "error.hpp"


void LTR303::readReg(uint8_t reg, uint8_t *data, size_t len)
{
	NrfI2C::write(LIGHT_DEVICE, LIGHT_DEVICE_ADDR, &reg, sizeof(reg), true);
	NrfI2C::read(LIGHT_DEVICE, LIGHT_DEVICE_ADDR, data, len);
}

void LTR303::writeReg(uint8_t address, uint8_t value)
{
    std::array<uint8_t, 2> buffer;
    buffer[0] = address;
    buffer[1] = value;

    NrfI2C::write(LIGHT_DEVICE, LIGHT_DEVICE_ADDR, buffer.data(), buffer.size(), false);
}

LTR303::LTR303() : Sensor("ALS") {
	uint8_t status_reg = 0;
	readReg(LTR303_ALS_STATUS_ADDR, &status_reg, sizeof(status_reg));
}

// Get the right lux algorithm
double LTR303::read(unsigned int gain)
{
    constexpr uint32_t integration_time = 1; // 100ms default
    const std::map<unsigned int, uint8_t> gain_map = {
   		{1, 0}, {2, 1}, {4, 2}, {8, 3}, {48, 6}, {96, 7}
    };

    // Set device into active mode with required gain setting
    uint8_t register_value;
    try {
    	register_value = (gain_map.at(gain) << 2) | 1;
    } catch (...) {
    	throw ErrorCode::BAD_GAIN_SETTING;
    }

    writeReg(LTR303_ALS_CONTR_ADDR, register_value);

    uint8_t status_reg = 0;
    do
    {
        nrf_delay_ms(1);
        readReg(LTR303_ALS_STATUS_ADDR, &status_reg, sizeof(status_reg));
    } while (!(status_reg & 0b0000100)); // Wait for new data

    uint8_t reg_ch0_data[2];
    uint8_t reg_ch1_data[2];

    // Channel 1 must be read before channel 0
    readReg(LTR303_ALS_DATA_CH1_0_ADDR, reg_ch1_data, sizeof(reg_ch1_data));
    readReg(LTR303_ALS_DATA_CH0_0_ADDR, reg_ch0_data, sizeof(reg_ch0_data));

    // Put into standby mode
	writeReg(LTR303_ALS_CONTR_ADDR, 0);

    uint32_t als_data_ch0 = (static_cast<uint16_t>(reg_ch0_data[1]) << 8) | (reg_ch0_data[0]);
    uint32_t als_data_ch1 = (static_cast<uint16_t>(reg_ch1_data[1]) << 8) | (reg_ch1_data[0]);

    if (als_data_ch0 >= 0xFFFF || als_data_ch1 >= 0xFFFF)
    {
        // At least one of our sensors has saturated so return the max value
        return 65535;
    }

    if (als_data_ch0 + als_data_ch1 == 0)
    {
        // We don't want to divide by 0 so treat this special case
    	return 0;
    }

    // Calculation
    double ratio = als_data_ch1 / (als_data_ch0 + als_data_ch1);
    double calculated;
    if (ratio < 0.45)
        calculated = (1774 * als_data_ch0 + 1106 * als_data_ch1);
    else if (ratio < 0.64)
        calculated = (4279 * als_data_ch0 - 1955 * als_data_ch1);
    else if (ratio < 0.85)
        calculated = (593 * als_data_ch0 + 119 * als_data_ch1);
    else
        calculated = 0;

    double lux = calculated / (double)gain / (double)integration_time;
    if (lux > 65535) {
        return 65535;
    }
    else
    {
        return lux;
    }
}
