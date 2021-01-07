#pragma once

#include "gps_scheduler.hpp"

class FakeGPSScheduler : public GPSScheduler {
public:
	void start() override;
	void stop() override;
	void notify_saltwater_switch_state(bool state) override;
};
