#pragma once

#include "bsp.hpp"
#include "gpio.hpp"
#include "uwdetector_service.hpp"

class SWSService : public UWDetectorService {
public:
	SWSService() : UWDetectorService("SaltwaterDetector") {}

private:
	bool detector_state() override {
		GPIOPins::set(SWS_ENABLE_PIN);
		PMU::delay_ms(m_enable_sample_delay); // Wait a while to allow for any water capacitance (this value is a total guess)
		bool new_state = GPIOPins::value(SWS_SAMPLE_PIN);
		GPIOPins::clear(SWS_ENABLE_PIN);
		return new_state;
	}

	bool service_is_enabled() override {
		bool enabled = service_read_param<bool>(ParamID::UNDERWATER_EN);
		BaseUnderwaterDetectSource src = service_read_param<BaseUnderwaterDetectSource>(ParamID::UNDERWATER_DETECT_SOURCE);
		return enabled && (src == BaseUnderwaterDetectSource::SWS);
	}
};
