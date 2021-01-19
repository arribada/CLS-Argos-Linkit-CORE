#pragma once

#include "comms_scheduler.hpp"

class FakeCommsScheduler : public CommsScheduler {
public:
	void start() override;
	void stop() override;
	void notify_saltwater_switch_state(bool state) override;
	void notify_sensor_log_update() override;
};

