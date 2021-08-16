#pragma once

#include <functional>
#include "switch.hpp"
#include "timer.hpp"
#include "scheduler.hpp"

enum class ReedSwitchGesture {
	ENGAGE,
	SHORT_HOLD,
	LONG_HOLD,
	RELEASE
};


class ReedSwitch {
private:
	Switch  &m_switch;
	uint64_t m_last_trigger_time;
	unsigned int m_short_hold_period_ms;
	unsigned int m_long_hold_period_ms;
	Scheduler::TaskHandle m_task;

	void switch_state_handler(bool);

protected:
	std::function<void(ReedSwitchGesture)> m_user_callback;

public:
	ReedSwitch(
			Switch &sw,
			unsigned int short_hold_period_ms = 3000,
			unsigned int long_hold_period_ms = 6000
			);
	~ReedSwitch();
	void start(std::function<void(ReedSwitchGesture)> func);
	void stop();
};
