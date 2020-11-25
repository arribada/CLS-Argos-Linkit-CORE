#ifndef __DEBUG_HPP_
#define __DEBUG_HPP_

#include "console_log.hpp"

#define DEBUG_ENABLE

#ifdef DEBUG_ENABLE

extern ConsoleLog *console_log;

#define DEBUG_ERROR(fmt, ...) do { if (console_log) console_log->error(fmt, ##__VA_ARGS__); } while (0)
#define DEBUG_WARN(fmt, ...)  do { if (console_log) console_log->warn(fmt, ##__VA_ARGS__); } while (0)
#define DEBUG_INFO(fmt, ...)  do { if (console_log) console_log->info(fmt, ##__VA_ARGS__); } while (0)
#define DEBUG_TRACE(fmt, ...) do { if (console_log) console_log->trace(fmt, ##__VA_ARGS__); } while (0)

#else

#define DEBUG_ERROR(fmt, ...)
#define DEBUG_WARN(fmt, ...)
#define DEBUG_INFO(fmt, ...)
#define DEBUG_TRACE(fmt, ...)

#endif

#endif // __DEBUG_HPP_
