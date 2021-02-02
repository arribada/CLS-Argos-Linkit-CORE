#ifndef __MOCK_M8Q_HPP_
#define __MOCK_M8Q_HPP_

#include "gps_scheduler.hpp"
#include "mock_std_function_comparator.hpp"

class MockM8Q : public GPSScheduler {
public:
	void power_on(std::function<void(GNSSData data)> data_notification_callback = nullptr) override {
        mock().installComparator("std::function<void()>", m_comparator);
        mock().actualCall("power_on").onObject(this).withParameterOfType("std::function<void()>", "data_notification_callback", &data_notification_callback);

        m_data_notification_callback = data_notification_callback;
    }

    void power_off() override {
        mock().actualCall("power_off").onObject(this);
    }

    std::function<void(GNSSData data)> m_data_notification_callback;

private:
	MockStdFunctionVoidComparator m_comparator;
};

#endif // __MOCK_M8Q_HPP_
