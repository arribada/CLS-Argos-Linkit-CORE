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
