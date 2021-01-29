#ifndef __NRFX_GPIOTE_H_
#define __NRFX_GPIOTE_H_

#include <map>
#include "nrf_err.h"

using nrfx_gpiote_pin_t = int;
using nrfx_gpiote_in_config_t = void;
enum nrf_gpiote_polarity_t {
	NRF_GPIOTE_POLARITY_LOTOHI,
	NRF_GPIOTE_POLARITY_HITOLO,
	NRF_GPIOTE_POLARITY_TOGGLE
};
using nrfx_gpiote_evt_handler_t = void(*)(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);


class GPIOTE {
private:
	static inline std::map<nrfx_gpiote_pin_t, nrfx_gpiote_evt_handler_t> m_map;
	static inline bool m_is_init = false;

public:
	static void nrfx_gpiote_in_event_enable(nrfx_gpiote_pin_t pin, bool int_enable)
	{
		(void)pin;
		(void)int_enable;
	}

	static void nrfx_gpiote_in_event_disable(nrfx_gpiote_pin_t pin)
	{
		(void)pin;
	}

	static nrfx_err_t nrfx_gpiote_in_init(nrfx_gpiote_pin_t               pin,
	                               	   	  nrfx_gpiote_in_config_t const * p_config,
										  nrfx_gpiote_evt_handler_t       evt_handler)
	{
		(void)p_config;
		m_map[pin] = evt_handler;
		return NRFX_SUCCESS;
	}

	static void nrfx_gpiote_in_uninit(nrfx_gpiote_pin_t               pin) {
		(void)pin;
	}

	static void trigger(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
		m_map[pin](pin, action);
	}

	static bool nrfx_gpiote_is_init() {
		return m_is_init;
	}

	static void nrfx_gpiote_uninit() {
		m_is_init = false;
	}

	static nrfx_err_t nrfx_gpiote_init() {
		m_is_init = true;
		return NRFX_SUCCESS;
	}
};


#define nrfx_gpiote_in_event_enable GPIOTE::nrfx_gpiote_in_event_enable
#define nrfx_gpiote_in_init GPIOTE::nrfx_gpiote_in_init
#define nrfx_gpiote_in_uninit GPIOTE::nrfx_gpiote_in_uninit
#define nrfx_gpiote_is_init GPIOTE::nrfx_gpiote_is_init
#define nrfx_gpiote_init GPIOTE::nrfx_gpiote_init
#define nrfx_gpiote_in_event_disable GPIOTE::nrfx_gpiote_in_event_disable
#define nrfx_gpiote_uninit GPIOTE::nrfx_gpiote_uninit

#endif // __NRFX_GPIOTE_H_
