#pragma once

#include "bsp.hpp"
#include "uwdetector.hpp"
#include "gpio.hpp"
#include "config_store.hpp"

extern ConfigurationStore *configuration_store;

class SWS : public UWDetector {
public:
	SWS(unsigned int sched_units = 1) : UWDetector(sched_units, 5, 1000) {}

private:
	bool detector_state() override {
		GPIOPins::set(SWS_ENABLE_PIN);
		PMU::delay_ms(1); // Wait a while to allow for any water capacitance (this value is a total guess)
		bool new_state = GPIOPins::value(SWS_SAMPLE_PIN);
		GPIOPins::clear(SWS_ENABLE_PIN);
		return new_state;
	}

	bool service_is_enabled() override {
		bool enabled = configuration_store->read_param<bool>(ParamID::UNDERWATER_EN);
		BaseUnderwaterDetectSource src = configuration_store->read_param<BaseUnderwaterDetectSource>(ParamID::UNDERWATER_DETECT_SOURCE);
		return enabled && (src == BaseUnderwaterDetectSource::SWS);
	}
};
