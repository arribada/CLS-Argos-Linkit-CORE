#include "reed.hpp"
#include "timer.hpp"
#include "debug.hpp"
#include "scheduler.hpp"

extern Timer *system_timer;
extern Scheduler *system_scheduler;

ReedSwitch::ReedSwitch(
		Switch &sw,
		unsigned int short_hold_period_ms,
		unsigned int long_hold_period_ms) : m_switch(sw)
{
	m_short_hold_period_ms = short_hold_period_ms;
	m_long_hold_period_ms = long_hold_period_ms;
}

ReedSwitch::~ReedSwitch()
{
	stop();
}

void ReedSwitch::start(std::function<void(ReedSwitchGesture)> func) {
	m_user_callback = func;
	m_switch.start([this](bool state) { switch_state_handler(state); });
}

void ReedSwitch::stop() {
	m_switch.stop();
}

void ReedSwitch::switch_state_handler(bool state) {

	uint64_t now = system_timer->get_counter();
	uint64_t delta = now - m_last_trigger_time;

	// Update last trigger time
	m_last_trigger_time = now;

	if (state) {

		DEBUG_TRACE("ReedSwitch::switch_state_handler: ENGAGE");

		if (m_user_callback) {
			system_scheduler->post_task_prio([this]() {
				m_user_callback(ReedSwitchGesture::ENGAGE);
			}, "ReedSwitchUserCallback");
		}

	} else {

		ReedSwitchGesture gesture = ReedSwitchGesture::RELEASE;
		if (delta >= m_long_hold_period_ms) {
			DEBUG_TRACE("ReedSwitch::switch_state_handler: LONG_HOLD");
			gesture = ReedSwitchGesture::LONG_HOLD;
		} else if (delta >= m_short_hold_period_ms) {
			DEBUG_TRACE("ReedSwitch::switch_state_handler: SHORT_HOLD");
			gesture = ReedSwitchGesture::SHORT_HOLD;
		} else {
			DEBUG_TRACE("ReedSwitch::switch_state_handler: RELEASE");
		}

		if (m_user_callback) {
			system_scheduler->post_task_prio([this, gesture]() {
				m_user_callback(gesture);
			}, "ReedSwitchUserCallback");
		}
	}
}
