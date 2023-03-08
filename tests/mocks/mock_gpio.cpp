#include "gpio.hpp"

#include "CppUTestExt/MockSupport.h"


void GPIOPins::initialise() {
	mock().actualCall("initialise");
}

void GPIOPins::set(uint32_t pin) {
	mock().actualCall("set").withParameter("pin", pin);
}

void GPIOPins::clear(uint32_t pin) {
	mock().actualCall("clear").withParameter("pin", pin);
}

uint32_t GPIOPins::value(uint32_t pin) {
	return mock().actualCall("value").withParameter("pin", pin).returnUnsignedLongIntValue();
}

void GPIOPins::toggle(uint32_t pin) {
	mock().actualCall("toggle").withParameter("pin", pin);
}

void GPIOPins::enable(uint32_t pin) {
	mock().actualCall("enable").withParameter("pin", pin);
}

void GPIOPins::disable(uint32_t pin) {
	mock().actualCall("disable").withParameter("pin", pin);
}
