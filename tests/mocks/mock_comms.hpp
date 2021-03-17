#ifndef __MOCK_COMMS_HPP_
#define __MOCK_COMMS_HPP_

#include "service_scheduler.hpp"
#include "debug.hpp"

class MockCommsScheduler : public ServiceScheduler {
public:
	void start(std::function<void(ServiceEvent)> data_notification_callback = nullptr) {
		(void)data_notification_callback;
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
