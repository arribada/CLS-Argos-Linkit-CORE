#ifndef __LOGGER_HPP_
#define __LOGGER_HPP_

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

#include "messages.hpp"

class Logger {

public:
	void warn(const char *msg, ...) {
		LogEntry buffer;
		va_list args;
		va_start (args, msg);
		vsprintf (buffer.data, msg, args);
		va_end (args);
		buffer.header.log_type = LOG_WARN;
		// TODO: set date time in header
		write(&buffer);
	}
	void error(const char *msg, ...) {
		LogEntry buffer;
		va_list args;
		va_start (args, msg);
		vsprintf (buffer.data, msg, args);
		va_end (args);
		buffer.header.log_type = LOG_ERROR;
		// TODO: set date time in header
		write(&buffer);
	}
	void info(const char *msg, ...) {
		LogEntry buffer;
		va_list args;
		va_start (args, msg);
		vsprintf (buffer.data, msg, args);
		va_end (args);
		buffer.header.log_type = LOG_INFO;
		// TODO: set date time in header
		write(&buffer);
	}
	void debug(const char *msg, ...) {
		LogEntry buffer;
		va_list args;
		va_start (args, msg);
		vsprintf (buffer.data, msg, args);
		va_end (args);
		buffer.header.log_type = LOG_DEBUG;
		// TODO: set date time in header
		write(&buffer);
	}
	virtual void write(void *);
	virtual void read(void *, int index = 0);
};

#endif // __LOGGER_HPP_
