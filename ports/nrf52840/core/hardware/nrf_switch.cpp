#include <functional>
#include <map>

#include "nrfx_gpiote.h"
#include "gpio.hpp"
#include "switch.hpp"
#include "nrf_switch.hpp"
#include "bsp.hpp"
#include "timer.hpp"
#include "error.hpp"
#include "debug.hpp"


extern Timer *system_timer;

// This class maps the pin number to an NrfSwitch object -- this is largely needed because
// the Nordic GPIOTE driver callback does not have a user-defined context and only passes
// back the pin number

class NrfSwitchManager {
private:
	static inline std::map<int, NrfSwitch *> m_map;

public:
	static void add(int pin, NrfSwitch *ref) {
		m_map.insert({pin, ref});
	}
	static void remove(int pin) {
		m_map.erase(pin);
	}
	static void process_event(int pin, bool state) {
		NrfSwitch *obj = m_map[pin];
		obj->process_event(state);
	}
	static int get_pin(int pin) {
		NrfSwitch *obj = m_map[pin];
		return obj->m_pin;
	}
};

// Handle events from GPIOTE here and propagate them back to corresponding NrfSwitch object
static void nrfx_gpiote_in_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
	switch (action)  {
	case NRF_GPIOTE_POLARITY_LOTOHI:
		NrfSwitchManager::process_event((int)pin, true);
		break;
	case NRF_GPIOTE_POLARITY_HITOLO:
		NrfSwitchManager::process_event((int)pin, false);
		break;
	case NRF_GPIOTE_POLARITY_TOGGLE:
		NrfSwitchManager::process_event((int)pin, (bool)GPIOPins::value(NrfSwitchManager::get_pin((int)pin)));
		break;
	default:
		break;
	}
}

NrfSwitch::NrfSwitch(int pin, unsigned int hysteresis_time_ms) : Switch(pin, hysteresis_time_ms) {
	// Initialise the library if it is not yet initialised
	if (!nrfx_gpiote_is_init())
		nrfx_gpiote_init();
}

NrfSwitch::~NrfSwitch() {
	stop();
}

void NrfSwitch::start(std::function<void(bool)> func) {
	Switch::start(func);
	NrfSwitchManager::add(BSP::GPIO_Inits[m_pin].pin_number, this);
	if (NRFX_SUCCESS != nrfx_gpiote_in_init(BSP::GPIO_Inits[m_pin].pin_number, &BSP::GPIO_Inits[m_pin].gpiote_in_config, nrfx_gpiote_in_event_handler)) {
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;
	}
	nrfx_gpiote_in_event_enable(BSP::GPIO_Inits[m_pin].pin_number, true);
}

void NrfSwitch::stop() {
	system_timer->cancel_schedule(m_timer_handle);
	nrfx_gpiote_in_event_disable(BSP::GPIO_Inits[m_pin].pin_number);
	nrfx_gpiote_in_uninit(BSP::GPIO_Inits[m_pin].pin_number);
	NrfSwitchManager::remove(BSP::GPIO_Inits[m_pin].pin_number);
	Switch::stop();
}

void NrfSwitch::update_state(bool state) {
	//DEBUG_TRACE("NrfSwitch::update_state: state=%u old=%u", state, m_current_state);
	// Call state change handler if state has changed
	if (state != m_current_state) {
		m_current_state = state;
		m_state_change_handler(state);
	}
}

void NrfSwitch::process_event(bool state) {
	uint64_t now = system_timer->get_counter();
	//DEBUG_TRACE("NrfSwitch::process_event: state=%u hysteresis=%u timer=%lu", state, m_hysteresis_time_ms, now);
	// Each time we get a new event we trigger the timer to post it after the hysteresis time.
	// If we receive another event, we cancel the previous task and start a new one.
	system_timer->cancel_schedule(m_timer_handle);
	m_timer_handle = system_timer->add_schedule(std::bind(&NrfSwitch::update_state, this, state),
			now + m_hysteresis_time_ms);
}
