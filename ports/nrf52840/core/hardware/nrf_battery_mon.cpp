#include <stdint.h>

#include "nrf_battery_mon.hpp"
#include "nrfx_saadc.h"
#include "bsp.hpp"
#include "error.hpp"
#include "debug.hpp"

#ifndef CPPUTEST
#include "crc16.h"
#else
#define crc16_compute(x, y, z)  0xFFFF
#endif

// Thresholds for low/critical battery filtering
#define CRITICIAL_V_THRESHOLD_MV	250
#define LOW_BATT_THRESHOLD			5

// ADC constants
#define ADC_MAX_VALUE (16384)      // 2^14
#define ADC_REFERENCE (0.6f)       // 0.6v internal reference

// LUT steps from 4.2V down to 3.2V in 0.1V steps
#define BATT_LUT_ENTRIES 11
#define BATT_LUT_MIN_V   3200
#define BATT_LUT_MAX_V   4200

static const constexpr uint8_t battery_voltage_lut[][BATT_LUT_ENTRIES] = {
	{ 100, 91, 79, 62, 42, 12, 2, 0, 0, 0, 0 },
	{ 100, 93, 84, 75, 64, 52, 22, 9, 0, 0, 0 },
	{ 100, 94, 83, 59, 50, 33, 15, 6, 0, 0, 0 }
};

static void nrfx_saadc_event_handler(nrfx_saadc_evt_t const *p_event)
{
	(void)p_event;
}

// These filtered values should be retained in noinit RAM so they can survive a reset
static __attribute__((section(".noinit"))) volatile uint16_t m_filtered_values[2];
static __attribute__((section(".noinit"))) volatile uint16_t m_crc;


NrfBatteryMonitor::NrfBatteryMonitor(uint8_t adc_channel,
		BatteryChemistry chem,
		uint16_t critical_voltage,
		uint8_t low_level
		) :
		BatteryMonitor(low_level, critical_voltage)
{
	// One-time initialise the driver (assumes we are the only instance)
    nrfx_saadc_init(&BSP::ADC_Inits.config, nrfx_saadc_event_handler);
    nrfx_saadc_calibrate_offset();

    DEBUG_TRACE("Enter ADC calibration...");
    // Wait for calibrate offset done event
    while (nrfx_saadc_is_busy()) // Wait for calibration to complete
    {}
    DEBUG_TRACE("ADC calibration complete..."); // NOTE: Calibration is retained until power reset. Init/uninit does not clear it

	nrfx_saadc_uninit();

    m_adc_channel = adc_channel;
    m_is_init = false;
    m_chem = chem;
}

float NrfBatteryMonitor::sample_adc()
{
    nrf_saadc_value_t raw = 0;

	// We need to init and uninit the SAADC peripheral here to reduce our sleep current
	
	nrfx_saadc_init(&BSP::ADC_Inits.config, nrfx_saadc_event_handler);
	nrfx_saadc_channel_init(m_adc_channel, &BSP::ADC_Inits.channel_config[m_adc_channel]);

    nrfx_saadc_sample_convert(m_adc_channel, &raw);
	
	nrfx_saadc_uninit();

    return ((float) raw) / ((ADC_GAIN / ADC_REFERENCE) * ADC_MAX_VALUE) * 1000.0f;
}

void NrfBatteryMonitor::internal_update() {
	// Sample ADC and convert values
	uint16_t mv = convert_voltage(sample_adc());
	uint8_t  level = convert_level(mv);

	// Check CRC of the previously stored filtered values
	uint16_t crc = crc16_compute((const uint8_t *)m_filtered_values, sizeof(m_filtered_values), nullptr);
	if (crc == m_crc) {
		// Previously filtered values are valid -- make sure we don't
		// allow filtered values to bounce around the threshold margin
		// thus causing events to re-trigger unless a sufficient exit
		// threshold is reached
		if (m_filtered_values[0] < m_critical_voltage_mv) {
			if (mv >= (m_critical_voltage_mv + CRITICIAL_V_THRESHOLD_MV))
				m_filtered_values[0] = mv;
		} else {
			m_filtered_values[0] = mv;
		}
		if (m_filtered_values[1] < m_low_level) {
			if (level >= (m_low_level + LOW_BATT_THRESHOLD))
				m_filtered_values[1] = level;
		} else {
			m_filtered_values[1] = level;
		}
	} else {
		// No previous values so set new values
		m_filtered_values[0] = mv;
		m_filtered_values[1] = level;
	}

	// Updated CRC in noinit RAM
	m_crc = crc16_compute((const uint8_t *)m_filtered_values, sizeof(m_filtered_values), nullptr);

	// Apply new values
	m_last_voltage_mv = mv;
	m_last_level = level;

	// Set flags
	m_is_critical_voltage = m_filtered_values[0] < m_critical_voltage_mv;
	m_is_low_level = m_filtered_values[1] < m_low_level;
}

uint8_t NrfBatteryMonitor::convert_level(uint16_t mV)
{
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

uint16_t NrfBatteryMonitor::convert_voltage(float adc)
{
#ifdef BATTERY_NOT_FITTED
	return BATT_LUT_MAX_V;
#else
	return (uint16_t)(adc * RP506_ADC_GAIN);
#endif
}
