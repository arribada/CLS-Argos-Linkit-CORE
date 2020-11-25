#ifndef __TIMER_HPP_
#define __TIMER_HPP_

extern "C" {
#include <stdint.h>
}

#define MAX_TIMER_SCHEDULES    32


using namespace std;

using TimerEvent = void(*)(void *);

struct TimerSchedule {
	void *user_arg;
	TimerEvent event;
	uint64_t target_counter_value;
};

static bool operator==(const TimerSchedule& lhs, const TimerSchedule& rhs)
{
    return (lhs.user_arg == rhs.user_arg &&
    		lhs.event == rhs.event &&
			lhs.target_counter_value == rhs.target_counter_value);
}

class Timer {
public:
	virtual uint64_t get_counter() = 0;
	virtual void add_schedule(TimerSchedule &s) = 0;
	virtual void cancel_schedule(TimerSchedule const &s) = 0;
	virtual void cancel_schedule(TimerEvent const &e) = 0;
	virtual void cancel_schedule(void *user_arg) = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
};

#endif // __TIMER_HPP_
