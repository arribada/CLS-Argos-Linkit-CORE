#ifndef __DEBUG_HPP_
#define __DEBUG_HPP_

#include "console_log.hpp"


#ifdef DEBUG_ENABLE

#define DEBUG_ERROR(fmt, ...) ConsoleLog::error(fmt, __VA_ARGS__)
#define DEBUG_WARN(fmt, ...)  ConsoleLog::warn(fmt, __VA_ARGS__)
#define DEBUG_INFO(fmt, ...)  ConsoleLog::info(fmt, __VA_ARGS__)
#define DEBUG_TRACE(fmt, ...) ConsoleLog::trace(fmt, __VA_ARGS__)

#else

#define DEBUG_ERROR(fmt, ...)
#define DEBUG_WARN(fmt, ...)
#define DEBUG_INFO(fmt, ...)
#define DEBUG_TRACE(fmt, ...)

#endif
