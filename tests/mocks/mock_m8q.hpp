#pragma once

#include "debug.hpp"
#include "logger.hpp"
#include "gps.hpp"
#include "timeutils.hpp"

class MockM8Q : public GPSDevice {
public:
    void notify_gnss_data(std::time_t time, double lat=0, double lon=0, double hdop = 0, double hacc = 0, bool valid = 1) {
    	GNSSData gnss_data;
    	gnss_data.lat = lat;
    	gnss_data.lon = lon;
    	gnss_data.valid = valid;
    	gnss_data.hDOP = hdop;
    	gnss_data.hAcc = hacc;
    	convert_datetime_to_epoch(time, gnss_data.year, gnss_data.month, gnss_data.day, gnss_data.hour, gnss_data.min, gnss_data.sec);
    	notify(GPSEventPVT(gnss_data));
    }

    void notify_power_off(bool fix_found ) {
        notify(GPSEventPowerOff(fix_found));
    }

    void power_on(const GPSNavSettings& nav_settings) override {
    	DEBUG_TRACE("MockM8Q::power_on()");
        mock().actualCall("power_on").onObject(this).withParameterOfType("const GPSNavSettings&", "nav_settings", &nav_settings);
    }

    void power_off() override {
    	DEBUG_TRACE("MockM8Q::power_off()");
        mock().actualCall("power_off").onObject(this);
    }
};
