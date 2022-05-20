#pragma once

#include <cstdint>
#include <optional>
#include "inplace_function.hpp"

#define MAX_NUM_TIMERS 32

#ifndef INPLACE_FUNCTION_SIZE_TIMER
#define INPLACE_FUNCTION_SIZE_TIMER 48
#endif

class Timer {
public:
	using TimerHandle = std::optional<unsigned int>;

	virtual ~Timer() {}
	virtual uint64_t get_counter() = 0;
	virtual TimerHandle add_schedule(stdext::inplace_function<void(), INPLACE_FUNCTION_SIZE_TIMER> const &task_func, uint64_t target_count) = 0;
	virtual void cancel_schedule(TimerHandle &handle) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
};
