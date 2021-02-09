#include <stdint.h>

#include "nrf_battery_mon.hpp"
#include "nrfx_saadc.h"
#include "bsp.hpp"
#include "error.hpp"
#include "debug.hpp"

// ADC constants
#define ADC_MAX_VALUE (16384)      // 2^14
#define ADC_REFERENCE (0.6f)       // 0.6v internal reference
#define ADC_GAIN      (1.0f/6.0f)  // 1/6 gain

// Battery voltage divider
#define VOLTAGE_DIV_R1 (1200000) // 1.2M
#define VOLTAGE_DIV_R2 (560000)  // 560k

// LUT steps from 4.2V down to 3.2V in 0.1V steps
#define BATT_LUT_ENTRIES 11
#define BATT_LUT_MIN_V   3200
#define BATT_LUT_MAX_V   4200

static const constexpr uint8_t battery_voltage_lut[][BATT_LUT_ENTRIES] = {
	{ 100, 91, 79, 62, 42, 12, 2, 0, 0, 0, 0 },
	{ 100, 93, 84, 75, 64, 52, 22, 9, 0, 0, 0 },
	{ 100, 94, 83, 59, 50, 33, 15, 6, 0, 0, 0 }
};


NrfBatteryMonitor::NrfBatteryMonitor(uint8_t adc_channel, BatteryChemistry chem)
{
	// One-time initialise the driver (assumes we are the only instance)
    nrfx_saadc_init(&BSP::ADC_Inits.saadc_config, [](nrfx_saadc_evt_t const * p_event){});
    nrfx_saadc_calibrate_offset();

    DEBUG_TRACE("Enter ADC calibration...");
    // Wait for calibrate offset done event
    while (nrfx_saadc_is_busy()) // Wait for calibration to complete
    {}
    DEBUG_TRACE("ADC calibration complete...");

    m_adc_channel = adc_channel;
    m_is_init = false;
    m_chem = chem;
}

NrfBatteryMonitor::~NrfBatteryMonitor()
{
	stop();
	nrfx_saadc_uninit();
}

void NrfBatteryMonitor::start()
{
	if (nrfx_saadc_channel_init(m_adc_channel, &BSP::ADC_Inits.saadc_config_channel_config[m_adc_channel]) != NRFX_SUCCESS)
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;
	m_is_init = true;
}

void NrfBatteryMonitor::stop()
{
	if (m_is_init) {
		nrfx_saadc_channel_uninit(m_adc_channel);
	}
}

float NrfBatteryMonitor::sample_adc()
{
    int16_t raw;

    nrfx_saadc_sample_convert(m_adc_channel, (nrf_saadc_value_t *) &raw);

    return ((float) raw) / ((ADC_GAIN / ADC_REFERENCE) * ADC_MAX_VALUE) * 1000.0f;
}

uint8_t NrfBatteryMonitor::get_level()
{
	uint16_t mV = get_voltage();
	// Convert to value between 0..100 based on LUT conversion
	int lut_index = (BATT_LUT_ENTRIES - 1) - ((mV / 100) - (BATT_LUT_MIN_V / 100));
	DEBUG_TRACE("NrfBatteryMonitor::get_level: mV = %u lut_index=%d", mV, lut_index);

	if (lut_index <= 0) {
		return battery_voltage_lut[(unsigned)m_chem][0];
	} else if (lut_index > (BATT_LUT_ENTRIES - 1)) {
		return battery_voltage_lut[(unsigned)m_chem][BATT_LUT_ENTRIES - 1];
	} else {
		// Linear extrapolation
		uint8_t upper = battery_voltage_lut[(unsigned)m_chem][lut_index-1];
		uint8_t lower = battery_voltage_lut[(unsigned)m_chem][lut_index];
		uint16_t upper_mV = BATT_LUT_MAX_V - ((lut_index-1)*100);
		float t = (upper_mV - mV) / 100.0f;
		float result = (float)upper + (t * ((float)lower - (float)upper));
		DEBUG_TRACE("NrfBatteryMonitor::get_level: upper_mV=%u upper=%u lower=%u t=%f r=%f", upper_mV, upper, lower, (double)t, (double)result);
		return (uint8_t)result;
	}
}

uint16_t NrfBatteryMonitor::get_voltage()
{
	//float adc = sample_adc();
	//return (adc * (VOLTAGE_DIV_R1 + VOLTAGE_DIV_R2)) / VOLTAGE_DIV_R2;
	return 4200;
}
