#ifndef __MOCK_LOCATION_HPP_
#define __MOCK_LOCATION_HPP_

#include "service_scheduler.hpp"
#include "mock_std_function_comparator.hpp"

class MockLocationScheduler : public ServiceScheduler {
public:
	void start(std::function<void()> data_notification_callback = nullptr) override {

		// Install a comparator for checking the equality of std::functions
		mock().installComparator("std::function<void()>", m_comparator);

		mock().actualCall("start").onObject(this).withParameterOfType("std::function<void()>", "data_notification_callback", &data_notification_callback);
	}
	void stop() override {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) override {
		DEBUG_TRACE("MockLocationScheduler: notify_saltwater_switch_state");
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
	void notify_sensor_log_update() override {
		mock().actualCall("notify_sensor_log_update").onObject(this);
	}

private:
	MockStdFunctionVoidComparator m_comparator;
};

#endif // __MOCK_LOCATION_HPP_
