#pragma once

#include "bsp.hpp"
#include "uwdetector.hpp"
#include "gpio.hpp"

class SWS : public UWDetector {
public:
	SWS(unsigned int sched_units = 1) : UWDetector(sched_units) {}
	bool detector_state() override {
		GPIOPins::set(SWS_ENABLE_PIN);
		PMU::delay_ms(1); // Wait a while to allow for any water capacitance (this value is a total guess)
		bool new_state = GPIOPins::value(SWS_SAMPLE_PIN);
		GPIOPins::clear(SWS_ENABLE_PIN);
		return new_state;
	}
};
