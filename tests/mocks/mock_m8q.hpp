#ifndef __MOCK_M8Q_HPP_
#define __MOCK_M8Q_HPP_

#include "gps_scheduler.hpp"
#include "mock_std_function_comparator.hpp"
#include "timeutils.hpp"


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

    void notify_gnss_data(std::time_t time, double lat=0, double lon=0) {
    	GNSSData gnss_data;
    	gnss_data.lat = lat;
    	gnss_data.lon = lon;
    	gnss_data.valid = 1;
    	convert_datetime_to_epoch(time, gnss_data.year, gnss_data.month, gnss_data.day, gnss_data.hour, gnss_data.min, gnss_data.sec);
    	m_data_notification_callback(gnss_data);
    }

private:
    std::function<void(GNSSData data)> m_data_notification_callback;
	MockStdFunctionVoidComparator m_comparator;
};

#endif // __MOCK_M8Q_HPP_
