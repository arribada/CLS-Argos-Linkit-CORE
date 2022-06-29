#pragma once

#include "sensor.hpp"
#include "uwdetector_service.hpp"

class PressureDetectorService : public UWDetectorService {
public:
	PressureDetectorService(Sensor& sensor) : UWDetectorService(1, 1, 0, "PressureDetector"), m_driver(sensor) {}

private:
	Sensor& m_driver;

	bool detector_state() override { return m_driver.read() >= m_activation_threshold; }

	bool service_is_enabled() override {
		bool enabled = service_read_param<bool>(ParamID::UNDERWATER_EN);
		BaseUnderwaterDetectSource src = service_read_param<BaseUnderwaterDetectSource>(ParamID::UNDERWATER_DETECT_SOURCE);
		return enabled && (src == BaseUnderwaterDetectSource::PRESSURE_SENSOR);
	}
};
