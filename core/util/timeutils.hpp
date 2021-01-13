#ifndef __TIMEUTILS_HPP_H
#define __TIMEUTILS_HPP_H

#include <ctime>
#include <stdint.h>

static std::time_t convert_epochtime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
	struct tm t;
	t.tm_sec = sec;
	t.tm_min = min;
	t.tm_hour = hour;
	t.tm_mday = day;
	t.tm_mon = month - 1;
	t.tm_year = year - 1900;
	std::time_t et = std::mktime(&t);
	return et;
}

#endif // __TIMEUTILS_HPP_H
