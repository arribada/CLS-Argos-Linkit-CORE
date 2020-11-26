#ifndef __MOCK_GPS_HPP_
#define __MOCK_GPS_HPP_

#include "gps_scheduler.hpp"

class MockGPSScheduler : public GPSScheduler {
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

#endif // __MOCK_GPS_HPP_
