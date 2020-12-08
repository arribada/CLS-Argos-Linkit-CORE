#ifndef __MOCK_TIMER_HPP_
#define __MOCK_TIMER_HPP_

#include "timer.hpp"

class MockTimer : public Timer {
public:

	uint64_t get_counter() override { return 0; }
	TimerHandle add_schedule(std::function<void()> const &, uint64_t) override { Timer::TimerHandle handle; return handle; }
	void cancel_schedule(TimerHandle &) override {
		mock().actualCall("cancel_schedule").onObject(this);
	}
	void start() override { mock().actualCall("start").onObject(this); }
	void stop() override { mock().actualCall("stop").onObject(this); }
};

#endif // __MOCK_TIMER_HPP_
