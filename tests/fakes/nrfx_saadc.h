#pragma once

#include <stdint.h>

#include "nrf_err.h"

using nrfx_saadc_config_t = void *;
using nrf_saadc_channel_config_t = void *;
using nrfx_saadc_event_handler_t = void *;
using nrf_saadc_value_t = int16_t;

class SAADC {
private:
	static inline int16_t m_adc_value = 0;
	static inline bool m_is_started = false;

public:
	static void set_adc_value(int16_t v) { m_adc_value = v; }
	static nrfx_err_t nrfx_saadc_init(nrfx_saadc_config_t const * p_config,
            nrfx_saadc_event_handler_t  event_handler) {
		(void)p_config;
		(void)event_handler;
		return NRFX_SUCCESS;
	}

	static nrfx_err_t nrfx_saadc_calibrate_offset() {
		return NRFX_SUCCESS;
	}

	static bool nrfx_saadc_is_busy() {
		return false;
	}

	static void nrfx_saadc_uninit() {
		m_is_started = false;
	}

	static nrfx_err_t nrfx_saadc_channel_init(uint8_t channel, nrf_saadc_channel_config_t const * const p_config) {
		(void)channel;
		(void)p_config;
		m_is_started = true;
		return NRFX_SUCCESS;
	}

	static nrfx_err_t nrfx_saadc_channel_uninit(uint8_t channel) {
		(void)channel;
		m_is_started = false;
		return NRFX_SUCCESS;
	}

	static nrfx_err_t nrfx_saadc_sample_convert(uint8_t channel, nrf_saadc_value_t * p_value) {
		(void)channel;
		if (m_is_started) {
			*p_value = m_adc_value;
			return NRFX_SUCCESS;
		}

		return NRFX_ERROR_INVALID_STATE;
	}
};

#define nrfx_saadc_init SAADC::nrfx_saadc_init
#define nrfx_saadc_calibrate_offset SAADC::nrfx_saadc_calibrate_offset
#define nrfx_saadc_is_busy SAADC::nrfx_saadc_is_busy
#define nrfx_saadc_uninit SAADC::nrfx_saadc_uninit
#define nrfx_saadc_channel_init SAADC::nrfx_saadc_channel_init
#define nrfx_saadc_channel_uninit SAADC::nrfx_saadc_channel_uninit
#define nrfx_saadc_sample_convert SAADC::nrfx_saadc_sample_convert

