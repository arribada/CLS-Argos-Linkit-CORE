#include "reed.hpp"
#include "timer.hpp"
#include "debug.hpp"
#include "scheduler.hpp"

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
	system_scheduler->cancel_task(m_task);
}

void ReedSwitch::switch_state_handler(bool state) {

	if (state) {

		DEBUG_TRACE("ReedSwitch::switch_state_handler: ENGAGE");

		if (m_user_callback) {
			system_scheduler->post_task_prio([this]() {
				m_user_callback(ReedSwitchGesture::ENGAGE);
			}, "ReedSwitchUserCallback");
		}

		// Start hold timers
		m_task = system_scheduler->post_task_prio([this]() {
			if (m_user_callback) {
				system_scheduler->post_task_prio([this]() {
					m_user_callback(ReedSwitchGesture::SHORT_HOLD);
				}, "ReedSwitchUserCallback");
			}

			m_task = system_scheduler->post_task_prio([this]() {
				if (m_user_callback) {
					system_scheduler->post_task_prio([this]() {
						m_user_callback(ReedSwitchGesture::LONG_HOLD);
					}, "ReedSwitchUserCallback");
				}
			}, "ShortHoldEventHandler", Scheduler::DEFAULT_PRIORITY, m_short_hold_period_ms);
		}, "LongHoldEventHandler", Scheduler::DEFAULT_PRIORITY, m_long_hold_period_ms - m_short_hold_period_ms);

	} else {

		DEBUG_TRACE("ReedSwitch::switch_state_handler: RELEASE");

		system_scheduler->cancel_task(m_task);

		if (m_user_callback) {
			system_scheduler->post_task_prio([this]() {
				m_user_callback(ReedSwitchGesture::RELEASE);
			}, "ReedSwitchUserCallback");
		}
	}
}
