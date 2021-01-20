#include <ctime>
#include "rtc.hpp"

class FakeRTC : public RTC {
public:
	std::time_t gettime() {
		return 0;
	}
	void settime(std::time_t t) {
		(void)t;
	}
};
