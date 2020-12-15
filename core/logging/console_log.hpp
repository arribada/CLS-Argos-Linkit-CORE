#ifndef __CONSOLE_LOG_HPP_
#define __CONSOLE_LOG_HPP_

#include "logger.hpp"
#include "messages.hpp"

using namespace std;


class ConsoleLog : public Logger {

private:
	void debug_formatter(const char *level, const char *msg) {
		printf("[%s] %s\r\n", level, msg);
	}
	void gps_formatter(const GPSLogEntry *gps) {
		const char *level = log_type_name[gps->header.log_type];
		printf("[%s] lat: %ld lon %ld\r\n", level, gps->lat, gps->lon);
	}

public:
	void create() override {}
	unsigned int num_entries() override {return 0;}
	void read(void *, int) override { }
	void write(void *entry) override {
		LogEntry *p = (LogEntry *)entry;
		switch (p->header.log_type) {
		case LOG_ERROR:
		case LOG_WARN:
		case LOG_INFO:
		case LOG_TRACE:
			debug_formatter(log_type_name[p->header.log_type], (const char * )p->data);
			break;
		case LOG_GPS:
			gps_formatter((const GPSLogEntry *)entry);
			break;
		default:
			// Not yet supported
			break;
		}
	}
};

#endif // __LOGGER_HPP_
