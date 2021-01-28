#include <functional>
#include <map>

#include "switch.hpp"
#include "nrf_switch.hpp"
#include "nrfx_gpiote.h"
#include "gpio.hpp"
#include "bsp.hpp"


class NrfSwitchManager {
private:
	static std::map<int, NrfSwitch *> m_map;

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
	if (!nrfx_gpiote_is_init())
		nrfx_gpiote_init();
}

NrfSwitch::~NrfSwitch() {
	stop();
}

void NrfSwitch::start(std::function<void(bool)> func) {
	Switch::start(func);
	NrfSwitchManager::add(m_pin, this);
	nrfx_gpiote_in_init(m_pin, &BSP::GPIO_Inits[m_pin].gpiote_in_config, nrfx_gpiote_in_event_handler);
	nrfx_gpiote_in_event_enable(m_pin, true);
}

void NrfSwitch::stop() {
	Switch::stop();
	nrfx_gpiote_in_event_disable(m_pin);
	nrfx_gpiote_in_uninit(m_pin);
	NrfSwitchManager::remove(m_pin);
}

void NrfSwitch::process_event(bool state) {
	// TODO
}
