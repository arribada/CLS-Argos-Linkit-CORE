#ifndef __DEBUG_HPP_
#define __DEBUG_HPP_

#include "logger.hpp"

class DebugLogger {
public:
	static inline Logger *console_log = nullptr;
	static inline Logger *system_log = nullptr;
};

#ifdef DEBUG_ENABLE

#if (DEBUG_LEVEL >= 1)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_ERROR(fmt, ...) \
	do { \
		if (DebugLogger::console_log) DebugLogger::console_log->error(fmt, ##__VA_ARGS__); \
		if (DebugLogger::system_log && DebugLogger::system_log->is_ready()) DebugLogger::system_log->error(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_ERROR(fmt, ...) \
do { \
	if (DebugLogger::console_log) DebugLogger::console_log->error(fmt, ##__VA_ARGS__); \
} while (0)
#endif
#else
#define DEBUG_ERROR(fmt, ...)
#endif

#if (DEBUG_LEVEL >= 2)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_WARN(fmt, ...) \
	do { \
		if (DebugLogger::console_log) DebugLogger::console_log->warn(fmt, ##__VA_ARGS__); \
		if (DebugLogger::system_log && DebugLogger::system_log->is_ready()) DebugLogger::system_log->warn(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_WARN(fmt, ...) \
	do { \
		if (DebugLogger::console_log) DebugLogger::console_log->warn(fmt, ##__VA_ARGS__); \
	} while (0)
#endif
#else
#define DEBUG_WARN(fmt, ...)
#endif

#if (DEBUG_LEVEL >= 3)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_INFO(fmt, ...) \
	do { \
		if (DebugLogger::console_log) DebugLogger::console_log->info(fmt, ##__VA_ARGS__); \
		if (DebugLogger::system_log && DebugLogger::system_log->is_ready()) DebugLogger::system_log->info(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_INFO(fmt, ...) \
	do { \
		if (DebugLogger::console_log) DebugLogger::console_log->info(fmt, ##__VA_ARGS__); \
	} while (0)
#endif
#else
#define DEBUG_INFO(fmt, ...)
#endif

// NOTE: Do not log TRACE level to system log as it can impact timing of some
// time critical areas of code
#if (DEBUG_LEVEL >= 4)
#define DEBUG_TRACE(fmt, ...) \
	do { \
		if (DebugLogger::console_log) DebugLogger::console_log->trace(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_TRACE(fmt, ...)
#endif


#else

#define DEBUG_ERROR(fmt, ...)
#define DEBUG_WARN(fmt, ...)
#define DEBUG_INFO(fmt, ...)
#define DEBUG_TRACE(fmt, ...)

#endif

#endif // __DEBUG_HPP_
