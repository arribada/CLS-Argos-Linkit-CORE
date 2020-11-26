#ifndef __MOCK_TIMER_HPP_
#define __MOCK_TIMER_HPP_

#include "timer.hpp"

class MockTimer : public Timer {
public:
	uint64_t get_counter() {
		//return mock().actualCall("get_counter").returnLongIntValue();
		return 0;
	}
	void add_schedule(TimerSchedule &s) {}
	void cancel_schedule(TimerSchedule const &s) {}
	void cancel_schedule(TimerEvent const &e) {}
	void cancel_schedule(void *user_arg) {}
	void start() {
		mock().actualCall("start").onObject(this);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
};

#endif // __MOCK_TIMER_HPP_
