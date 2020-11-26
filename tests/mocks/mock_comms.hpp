#ifndef __MOCK_COMMS_HPP_
#define __MOCK_COMMS_HPP_

#include "comms_scheduler.hpp"

class MockCommsScheduler : public CommsScheduler {
public:
	void start() {
		mock().actualCall("start").onObject(this);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) {
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
};

#endif // __MOCK_COMMS_HPP_
