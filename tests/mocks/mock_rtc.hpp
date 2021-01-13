#ifndef __MOCK_RTC_HPP_
#define __MOCK_RTC_HPP_

#include <ctime>
#include "rtc.hpp"


class MockRTC : public RTC {
public:
	std::time_t gettime() {
		return (std::time_t)mock().actualCall("gettime").onObject(this).returnUnsignedLongIntValue();
	}
	void settime(std::time_t t) {
		mock().actualCall("settime").onObject(this).withParameter("t", (uint64_t)t);
	}
};

#endif // __MOCK_RTC_HPP_
