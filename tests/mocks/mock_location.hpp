#ifndef __MOCK_LOCATION_HPP_
#define __MOCK_LOCATION_HPP_

#include "mock_comparators.hpp"
#include "service_scheduler.hpp"

class MockLocationScheduler : public ServiceScheduler {
public:
	void start(std::function<void(ServiceEvent&)> data_notification_callback = nullptr) override {

		// Install a comparator for checking the equality of std::functions
		mock().installComparator("std::function<void(ServiceEvent&)>", m_comparator);

		mock().actualCall("start").onObject(this).withParameterOfType("std::function<void(ServiceEvent&)>", "data_notification_callback", &data_notification_callback);
	}
	void stop() override {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) override {
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
	void notify_sensor_log_update() override {
		mock().actualCall("notify_sensor_log_update").onObject(this);
	}

private:
	MockStdFunctionServiceEventComparator m_comparator;
};

#endif // __MOCK_LOCATION_HPP_
