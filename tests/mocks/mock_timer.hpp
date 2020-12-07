#ifndef __MOCK_TIMER_HPP_
#define __MOCK_TIMER_HPP_

#include "timer.hpp"

class MockTimer : public Timer {
public:

	uint64_t get_counter() override { return 0; }
	TimerHandle add_schedule(std::function<void()> const &task_func, uint64_t target_count) override { Timer::TimerHandle handle; return handle; }
	void cancel_schedule(TimerHandle &handle) override {}
	void start() override { mock().actualCall("start").onObject(this); }
	void stop() override { mock().actualCall("stop").onObject(this); }
};

#endif // __MOCK_TIMER_HPP_
