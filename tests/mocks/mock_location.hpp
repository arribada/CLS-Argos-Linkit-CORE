#ifndef __MOCK_LOCATION_HPP_
#define __MOCK_LOCATION_HPP_

#include "gps_scheduler.hpp"
#include "mock_std_function_comparator.hpp"

class MockLocationScheduler : public LocationScheduler {
public:
	void start(std::function<void()> data_notification_callback = nullptr) {

		// Install a comparator for checking the equality of std::functions
		mock().installComparator("std::function<void()>", m_comparator);

		mock().actualCall("start").onObject(this).withParameterOfType("std::function<void()>", "data_notification_callback", &data_notification_callback);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) {
		DEBUG_TRACE("MockGPSScheduler: notify_saltwater_switch_state");
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}

private:
	MockStdFunctionVoidComparator m_comparator;
};

#endif // __MOCK_LOCATION_HPP_
