#ifndef __MOCK_GPS_HPP_
#define __MOCK_GPS_HPP_

#include "gps_scheduler.hpp"

class FakeGPSScheduler : public GPSScheduler {

public:
	MockStdFunctionVoidComparator m_comparator;

	void power_off() override {
		
	}

	void power_on(std::function<void(GNSSData data)> data_notification_callback = nullptr) override	{
		
	}
};

#endif // __MOCK_GPS_HPP_
