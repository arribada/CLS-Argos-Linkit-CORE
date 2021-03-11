#ifndef __LOGGER_HPP_
#define __LOGGER_HPP_

#include <cstring>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include "messages.hpp"
#include "rtc.hpp"
#include "timeutils.hpp"

extern RTC *rtc;

enum LogLevel {
	LOG_LEVEL_OFF,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_WARN,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG
};


class Logger {

private:
	int m_log_level = LOG_LEVEL_DEBUG;

	static inline void sync_datetime(LogHeader &header) {
#ifdef DEBUG_USING_RTC
		if (rtc) {
			uint16_t year;
			convert_datetime_to_epoch(rtc->gettime(), year, header.month, header.day, header.hours, header.minutes, header.seconds);
			header.year = year;
		}
		else
#endif
		{
			header.year = header.month = header.day = header.hours = header.minutes = header.seconds = 0;
		}
	}

public:
	virtual ~Logger() {}

	void set_log_level(int level) {
		m_log_level = level;
	}
	void warn(const char *msg, ...) {
		if (m_log_level >= LOG_LEVEL_WARN) {
			LogEntry buffer;
			va_list args;
			va_start(args, msg);
			vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
			va_end(args);
			buffer.header.log_type = LOG_WARN;
			buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
			sync_datetime(buffer.header);
			write(&buffer);
		}
	}
	void error(const char *msg, ...) {
		if (m_log_level >= LOG_LEVEL_ERROR) {
			LogEntry buffer;
			va_list args;
			va_start(args, msg);
			vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
			va_end(args);
			buffer.header.log_type = LOG_ERROR;
			buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
			sync_datetime(buffer.header);
			write(&buffer);
		}
	}
	void info(const char *msg, ...) {
		if (m_log_level >= LOG_LEVEL_INFO) {
			LogEntry buffer;
			va_list args;
			va_start(args, msg);
			vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
			va_end(args);
			buffer.header.log_type = LOG_INFO;
			buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
			sync_datetime(buffer.header);
			write(&buffer);
		}
	}
	void trace(const char *msg, ...) {
		if (m_log_level >= LOG_LEVEL_INFO) {
			LogEntry buffer;
			va_list args;
			va_start(args, msg);
			vsnprintf(reinterpret_cast<char*>(buffer.data), sizeof(buffer.data), msg, args);
			va_end(args);
			buffer.header.log_type = LOG_TRACE;
			buffer.header.payload_size = std::strlen(reinterpret_cast<char*>(buffer.data));
			sync_datetime(buffer.header);
			write(&buffer);
		}
	}
	virtual void create() = 0;
	virtual void write(void *) = 0;
	virtual void read(void *, int index = 0) = 0;
	virtual unsigned int num_entries() = 0;
	virtual bool is_ready() = 0;
};

#endif // __LOGGER_HPP_
