#include "reed.hpp"
#include "timer.hpp"
#include "debug.hpp"
#include "scheduler.hpp"

extern Timer *system_timer;
extern Scheduler *system_scheduler;

ReedSwitch::ReedSwitch(
		Switch &sw,
		unsigned int max_inter_swipe_period_ms,
		unsigned int short_hold_period_ms,
		unsigned int long_hold_period_ms) : m_switch(sw)
{
	m_max_inter_swipe_period_ms = max_inter_swipe_period_ms;
	m_short_hold_period_ms = short_hold_period_ms;
	m_long_hold_period_ms = long_hold_period_ms;
}

ReedSwitch::~ReedSwitch()
{
	stop();
}

void ReedSwitch::start(std::function<void(ReedSwitchGesture)> func) {
	m_trigger_count = 0;
	m_user_callback = func;
	m_switch.start([this](bool state) { switch_state_handler(state); });
}

void ReedSwitch::stop() {
	system_timer->cancel_schedule(m_timer);
	m_switch.stop();
}

void ReedSwitch::switch_state_handler(bool state) {

	uint64_t now = system_timer->get_counter();
	uint64_t delta = now - m_last_trigger_time;

	// Update last trigger time
	m_last_trigger_time = now;

	if (state) {
		m_trigger_count++;

		DEBUG_TRACE("ReedSwitch::switch_state_handler: trigger_count=%u", m_trigger_count);

		if (m_trigger_count == 1) {
			// If this is the first trigger then we start a hold timer
			m_timer = system_timer->add_schedule([this]() {
				// Trigger the short hold event and now start a new timer
				// for the long hold event
				DEBUG_TRACE("ReedSwitch::switch_state_handler: SHORT_HOLD");
				if (m_user_callback) {
					system_scheduler->post_task_prio([this]() {
						m_user_callback(ReedSwitchGesture::SHORT_HOLD);
					}, "ReedSwitchUserCallback");
				}
				m_timer = system_timer->add_schedule([this]() {
					DEBUG_TRACE("ReedSwitch::switch_state_handler: LONG_HOLD");
					// Trigger the long hold event
					if (m_user_callback) {
						system_scheduler->post_task_prio([this]() {
							m_user_callback(ReedSwitchGesture::LONG_HOLD);
						}, "ReedSwitchUserCallback");
					}
				}, system_timer->get_counter() - m_short_hold_period_ms + m_long_hold_period_ms);
			}, now + m_short_hold_period_ms);
		}

	} else {
		// Cancel any timer running
		system_timer->cancel_schedule(m_timer);

		// If the delta time exceeds the short hold period then this is a RELEASE event
		if (delta >= m_short_hold_period_ms) {
			DEBUG_TRACE("ReedSwitch::switch_state_handler: RELEASE");
			m_trigger_count = 0;
			if (m_user_callback) {
				system_scheduler->post_task_prio([this]() {
					m_user_callback(ReedSwitchGesture::RELEASE);
				}, "ReedSwitchUserCallback");
			}
			return;
		}

		DEBUG_TRACE("ReedSwitch::switch_state_handler: SWIPE");

		// Otherwise this is a SWIPE event, we start the inter-swipe timer to allow for
		// as many swipes as needed
		m_timer = system_timer->add_schedule([this]() {
			ReedSwitchGesture gesture;
			if (m_trigger_count == 1)
				gesture = ReedSwitchGesture::SINGLE_SWIPE;
			else if (m_trigger_count == 2)
				gesture = ReedSwitchGesture::DOUBLE_SWIPE;
			else if (m_trigger_count == 3)
				gesture = ReedSwitchGesture::TRIPLE_SWIPE;
			else {
				DEBUG_TRACE("ReedSwitch::switch_state_handler: SWIPE=%u exceeds limit!", m_trigger_count);
				m_trigger_count = 0;
				return;
			}

			DEBUG_TRACE("ReedSwitch::switch_state_handler: SWIPE=%u", m_trigger_count);
			m_trigger_count = 0;
			if (m_user_callback) {
				system_scheduler->post_task_prio([this, gesture]() {
					m_user_callback(gesture);
				}, "ReedSwitchUserCallback");
			}
		}, now + m_max_inter_swipe_period_ms);
	}
}
