#include <functional>
#include <map>

#include "nrfx_gpiote.h"
#include "switch.hpp"
#include "nrf_switch.hpp"
#include "gpio.hpp"
#include "bsp.hpp"
#include "scheduler.hpp"
#include "error.hpp"
#include "debug.hpp"


extern Scheduler *system_scheduler;
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
		NrfSwitchManager::process_event((int)pin, (bool)GPIOPins::value((uint32_t)pin));
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
	NrfSwitchManager::add(m_pin, this);
	if (NRFX_SUCCESS != nrfx_gpiote_in_init(m_pin, &BSP::GPIO_Inits[m_pin].gpiote_in_config, nrfx_gpiote_in_event_handler)) {
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;
	}
	nrfx_gpiote_in_event_enable(m_pin, true);
}

void NrfSwitch::stop() {
	system_scheduler->cancel_task(m_task_handle);
	nrfx_gpiote_in_event_disable(m_pin);
	nrfx_gpiote_in_uninit(m_pin);
	NrfSwitchManager::remove(m_pin);
	Switch::stop();
}

void NrfSwitch::update_state(bool state) {
	DEBUG_TRACE("NrfSwitch::update_state: state=%u old=%u", state, m_current_state);
	// Call state change handler if state has changed
	if (state != m_current_state) {
		m_current_state = state;
		m_state_change_handler(state);
	}
}

void NrfSwitch::process_event(bool state) {
	DEBUG_TRACE("NrfSwitch::process_event: state=%u hysteresis=%u timer=%lu", state, m_hysteresis_time_ms, system_timer->get_counter());
	// Each time we get a new event we trigger the task to post it after the hysteresis time.
	// If we receive another event, we cancel the previous task and start a new one.
	system_scheduler->cancel_task(m_task_handle);
	m_task_handle = system_scheduler->post_task_prio(std::bind(&NrfSwitch::update_state, this, state), Scheduler::DEFAULT_PRIORITY, m_hysteresis_time_ms);
}
