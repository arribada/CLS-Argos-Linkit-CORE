#ifndef __FAKE_RTC_HPP_
#define __FAKE_RTC_HPP_

#include <ctime>
#include "rtc.hpp"

class FakeRTC : public RTC {
public:
    FakeRTC() : m_time(0) {}

	std::time_t gettime() {
		return m_time;
	}
	void settime(std::time_t t) {
		m_time = t;
	}
    void incrementtime(std::time_t t)
    {
        m_time += t;
    }

private:
    std::time_t m_time;
};

#endif // __FAKE_RTC_HPP_
