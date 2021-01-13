#ifndef __RTC_HPP_
#define __RTC_HPP_

#include <ctime>

class RTC {
public:
	virtual ~RTC() {}
	virtual std::time_t  gettime() = 0;
	virtual void settime(std::time_t ) = 0;
};

#endif // __RTC_HPP_
