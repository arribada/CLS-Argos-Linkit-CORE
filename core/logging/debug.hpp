#ifndef __DEBUG_HPP_
#define __DEBUG_HPP_

#include "logger.hpp"

#ifdef DEBUG_ENABLE

extern Logger *console_log;
extern Logger *system_log;

#if (DEBUG_LEVEL >= 1)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_ERROR(fmt, ...) \
	do { \
		if (console_log) console_log->error(fmt, ##__VA_ARGS__); \
		if (system_log) system_log->error(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_ERROR(fmt, ...) \
do { \
	if (console_log) console_log->error(fmt, ##__VA_ARGS__); \
} while (0)
#endif
#else
#define DEBUG_ERROR(fmt, ...)
#endif

#if (DEBUG_LEVEL >= 2)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_WARN(fmt, ...) \
	do { \
		if (console_log) console_log->warn(fmt, ##__VA_ARGS__); \
		if (system_log) system_log->warn(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_WARN(fmt, ...) \
	do { \
		if (console_log) console_log->warn(fmt, ##__VA_ARGS__); \
	} while (0)
#endif
#else
#define DEBUG_WARN(fmt, ...)
#endif

#if (DEBUG_LEVEL >= 3)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_INFO(fmt, ...) \
	do { \
		if (console_log) console_log->info(fmt, ##__VA_ARGS__); \
		if (system_log) system_log->info(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_INFO(fmt, ...) \
	do { \
		if (console_log) console_log->info(fmt, ##__VA_ARGS__); \
	} while (0)
#endif
#else
#define DEBUG_INFO(fmt, ...)
#endif

#if (DEBUG_LEVEL >= 4)
#ifdef DEBUG_TO_SYSTEMLOG
#define DEBUG_TRACE(fmt, ...) \
	do { \
		if (console_log) console_log->trace(fmt, ##__VA_ARGS__); \
		if (system_log) system_log->trace(fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DEBUG_TRACE(fmt, ...) \
	do { \
		if (console_log) console_log->trace(fmt, ##__VA_ARGS__); \
	} while (0)
#endif
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
