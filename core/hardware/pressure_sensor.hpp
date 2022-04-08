#pragma once

#include "uwdetector.hpp"
#include "config_store.hpp"

extern ConfigurationStore *configuration_store;

class PressureSensor : public UWDetector {
public:
	PressureSensor() : UWDetector() {}

private:
	bool service_is_enabled() override {
		bool enabled = configuration_store->read_param<bool>(ParamID::UNDERWATER_EN);
		BaseUnderwaterDetectSource src = configuration_store->read_param<BaseUnderwaterDetectSource>(ParamID::UNDERWATER_DETECT_SOURCE);
		return enabled && (src == BaseUnderwaterDetectSource::PRESSURE_SENSOR);
	}
};
