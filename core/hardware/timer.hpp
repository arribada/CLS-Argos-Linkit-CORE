#pragma once

#include <cstdint>
#include <optional>
#include <functional>

class Timer {
public:
	static const unsigned int MAX_TIMERS = 32;

	using TimerHandle = std::optional<unsigned int>;

	virtual ~Timer() {}
	virtual uint64_t get_counter() = 0;
	virtual TimerHandle add_schedule(std::function<void()> const &task_func, uint64_t target_count) = 0;
	virtual void cancel_schedule(TimerHandle &handle) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
};
