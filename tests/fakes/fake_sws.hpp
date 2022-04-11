#pragma once

#include "uwdetector.hpp"

class FakeSWS : public UWDetector {
public:
	void set_state(bool state) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_LOG_UPDATED;
		e.event_data = state;
		e.event_source = ServiceIdentifier::UW_SENSOR;
		ServiceManager::inject_event(e);
	}

private:
	bool service_is_enabled() override { return false; }

};
