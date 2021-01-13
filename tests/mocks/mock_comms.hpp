#ifndef __MOCK_COMMS_HPP_
#define __MOCK_COMMS_HPP_

#include "comms_scheduler.hpp"
#include "debug.hpp"

class MockCommsScheduler : public CommsScheduler {
public:
	void start() {
		mock().actualCall("start").onObject(this);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) {
		DEBUG_TRACE("MockCommsScheduler: notify_saltwater_switch_state");
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
	void notify_sensor_log_update() {
		mock().actualCall("notify_sensor_log_update").onObject(this);
	}
};

#endif // __MOCK_COMMS_HPP_
