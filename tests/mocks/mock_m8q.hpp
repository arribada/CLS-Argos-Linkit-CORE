#pragma once

#include "debug.hpp"
#include "logger.hpp"
#include "gps_service.hpp"
#include "timeutils.hpp"

class MockM8Q : public GPSService {
public:
	MockM8Q(Logger *logger) : GPSService(logger) {}
    void notify_gnss_data(std::time_t time, double lat=0, double lon=0, double hdop = 0, double hacc = 0) {
    	GNSSData gnss_data;
    	gnss_data.lat = lat;
    	gnss_data.lon = lon;
    	gnss_data.valid = 1;
    	gnss_data.hDOP = hdop;
    	gnss_data.hAcc = hacc;
    	convert_datetime_to_epoch(time, gnss_data.year, gnss_data.month, gnss_data.day, gnss_data.hour, gnss_data.min, gnss_data.sec);
    	m_data_notification_callback(gnss_data);
    }

private:
    std::function<void(GNSSData data)> m_data_notification_callback;

    void power_on(const GPSNavSettings& nav_settings, std::function<void(GNSSData data)> data_notification_callback = nullptr) override {
    	DEBUG_TRACE("MockM8Q::power_on()");
        mock().actualCall("power_on").onObject(this).withParameterOfType("const GPSNavSettings&", "nav_settings", &nav_settings).withParameterOfType("std::function<void()>", "data_notification_callback", &data_notification_callback);
        m_data_notification_callback = data_notification_callback;
    }

    void power_off() override {
    	DEBUG_TRACE("MockM8Q::power_off()");
        mock().actualCall("power_off").onObject(this);
    }

};
