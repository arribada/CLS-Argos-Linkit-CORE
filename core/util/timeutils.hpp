#ifndef __TIMEUTILS_HPP_H
#define __TIMEUTILS_HPP_H

#include <ctime>
#include <stdint.h>

static std::time_t convert_epochtime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
	struct tm t;
	memset(&t, 0, sizeof(t));
	t.tm_sec = sec;
	t.tm_min = min;
	t.tm_hour = hour;
	t.tm_mday = day;
	t.tm_mon = month - 1;
	t.tm_year = year - 1900;
	std::time_t et = std::mktime(&t);

	return et;
}

static void convert_day_of_year(const uint16_t year, const uint8_t day_of_year, uint8_t& month, uint16_t& day) {
	std::time_t t = convert_epochtime(year, 1, 1, 0, 0, 0); // Epoch time at start of year i.e., 1st January
	t += (day_of_year - 1) * 24 * 3600;   // Add day of year in seconds, note: we subtract 1 because day of year is 1...365
	struct tm *tm = std::gmtime(&t); // Convert back to struct tm

	// Return back to caller the month and day of month
	month = tm->tm_mon + 1;
	day = tm->tm_mday;
}

static void convert_datetime_to_epoch(std::time_t time, uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &min, uint8_t &sec)
{
	std::tm *date_time = std::gmtime(&time);

    day = date_time->tm_mday;
    month = date_time->tm_mon + 1;
    year = date_time->tm_year + 1900;
    hour = date_time->tm_hour;
    min = date_time->tm_min;
    sec = date_time->tm_sec;
}

#endif // __TIMEUTILS_HPP_H
