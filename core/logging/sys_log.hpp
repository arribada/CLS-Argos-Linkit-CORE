#pragma once

#include <string>
#include "logger.hpp"


class SysLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,log_level,message\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(e.header.year, e.header.month, e.header.day, e.header.hours, e.header.minutes, e.header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		snprintf(entry, sizeof(entry), "%s,%s,%s\r\n",
				d1,
				log_level_str(e.header.log_type),
				e.data);
		return std::string(entry);
	}
};
