#ifndef __FAKE_RTC_HPP_
#define __FAKE_RTC_HPP_

#include <ctime>
#include "rtc.hpp"

class FakeRTC : public RTC {
public:
    FakeRTC() : m_time(0), m_is_set(false) { }

	std::time_t gettime() {
		return m_time;
	}
	void settime(std::time_t t) {
		m_time = t;
		m_is_set = true;
	}
    void incrementtime(std::time_t t)
    {
        m_time += t;
    }
    bool is_set() {
    	return m_is_set;
    }

private:
    std::time_t m_time;
    bool m_is_set;
};

#endif // __FAKE_RTC_HPP_
