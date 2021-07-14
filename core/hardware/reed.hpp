#pragma once

#include <functional>
#include "switch.hpp"
#include "timer.hpp"

enum class ReedSwitchGesture {
	SINGLE_SWIPE,
	DOUBLE_SWIPE,
	TRIPLE_SWIPE,
	SHORT_HOLD,
	LONG_HOLD,
	RELEASE
};


class ReedSwitch {
private:
	Switch  &m_switch;
	uint64_t m_last_trigger_time;
	unsigned int m_trigger_count;
	unsigned int m_max_inter_swipe_period_ms;
	unsigned int m_short_hold_period_ms;
	unsigned int m_long_hold_period_ms;
	std::function<void(ReedSwitchGesture)> m_user_callback;
	Timer::TimerHandle m_timer;

	void switch_state_handler(bool);

public:
	ReedSwitch(
			Switch &sw,
			unsigned int max_inter_swipe_period_ms = 500,
			unsigned int short_hold_period_ms = 3000,
			unsigned int long_hold_period_ms = 10000
			);
	~ReedSwitch();
	void start(std::function<void(ReedSwitchGesture)> func);
	void stop();
};
