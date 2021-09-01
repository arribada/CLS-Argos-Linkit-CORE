#include <functional>
#include <map>

#include "error.hpp"
#include "bsp.hpp"
#include "nrf_irq.hpp"
#include "gpio.hpp"
#include "nrfx_gpiote.h"

// This class maps the pin number to an NrfIRQ object -- this is largely needed because
// the Nordic GPIOTE driver callback does not have a user-defined context and only passes
// back the pin number

class NrfIRQManager {
private:
	static inline std::map<int, NrfIRQ *> m_map;

public:
	static void add(int pin, NrfIRQ *ref) {
		m_map.insert({pin, ref});
	}
	static void remove(int pin) {
		m_map.erase(pin);
	}
	static void process_event(int pin) {
		NrfIRQ *obj = m_map[pin];
		obj->process_event();
	}
	static int get_pin(int pin) {
		NrfIRQ *obj = m_map[pin];
		return obj->m_pin;
	}
};


// Handle events from GPIOTE here and propagate them back to corresponding NrfSwitch object
static void nrfx_gpiote_in_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
	switch (action)  {
	case NRF_GPIOTE_POLARITY_LOTOHI:
	case NRF_GPIOTE_POLARITY_HITOLO:
		NrfIRQManager::process_event((int)pin);
		break;
	case NRF_GPIOTE_POLARITY_TOGGLE:
	default:
		break;
	}
}

NrfIRQ::NrfIRQ(int pin) {
	m_pin = pin;
	// Initialise the library if it is not yet initialised
	if (!nrfx_gpiote_is_init())
		nrfx_gpiote_init();
	// Install the pin global handler
	if (NRFX_SUCCESS != nrfx_gpiote_in_init(BSP::GPIO_Inits[m_pin].pin_number, &BSP::GPIO_Inits[m_pin].gpiote_in_config, nrfx_gpiote_in_event_handler)) {
		throw ErrorCode::RESOURCE_NOT_AVAILABLE;
	}
	// Install the pin into the IRQ manager
	NrfIRQManager::add(BSP::GPIO_Inits[m_pin].pin_number, this);
}

NrfIRQ::~NrfIRQ() {
	disable();
	nrfx_gpiote_in_uninit(BSP::GPIO_Inits[m_pin].pin_number);
	NrfIRQManager::remove(BSP::GPIO_Inits[m_pin].pin_number);
}

void NrfIRQ::enable(std::function<void()> func) {
	m_func = func;
	nrfx_gpiote_in_event_enable(BSP::GPIO_Inits[m_pin].pin_number, true);
}

void NrfIRQ::disable() {
	nrfx_gpiote_in_event_disable(BSP::GPIO_Inits[m_pin].pin_number);
}

void NrfIRQ::process_event() {
	m_func();
}

bool NrfIRQ::poll() {
	if (BSP::GPIO_Inits[m_pin].gpiote_in_config.sense == NRF_GPIOTE_POLARITY_LOTOHI)
		return GPIOPins::value(m_pin);
	return !GPIOPins::value(m_pin);
}
