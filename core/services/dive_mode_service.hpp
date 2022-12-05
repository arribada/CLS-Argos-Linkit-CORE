#pragma once

#include "service.hpp"
#include "switch.hpp"
#include "irq.hpp"


class DiveModeService : public Service {
public:
	enum class DiveState {
		Idle,
		StartPending,
		Engaged,
		Disengaged
	};

	DiveModeService(Switch& reed, IRQ& wchg) : Service(ServiceIdentifier::DIVE_MODE, "DIVE", nullptr),
		m_reed(reed), m_wchg(wchg), m_dive_state(DiveState::Idle) {}

	void notify_peer_event(ServiceEvent& e) {
		// Notify intent to start dive mode if the following conditions arise:
		// 1. UW sensor is active
		// 2. Dive state is idle
		// 3. Service is enabled
		if (e.event_source == ServiceIdentifier::UW_SENSOR &&
			e.event_type == ServiceEventType::SERVICE_LOG_UPDATED &&
			std::get<bool>(e.event_data) &&
			service_is_enabled() &&
			m_dive_state == DiveState::Idle) {
			// Enter start pending state and reschedule the service which will call us
			// back after the dive mode start time period has elapsed
			m_dive_state = DiveState::StartPending;
			service_reschedule();
		}
	}

private:
	Switch& m_reed;
	IRQ& m_wchg;
	DiveState m_dive_state;

protected:
	void service_initiate() {
		// If dive state is start pending then engage the dive state
		// and pause any reed switch activity
		if (m_dive_state == DiveState::StartPending) {
			DEBUG_INFO("DiveModeService: dive mode engaged");
			m_dive_state = DiveState::Engaged;
			m_reed.pause();
		}
	}

	unsigned int service_next_schedule_in_ms() override {
		// If dive mode start is pending then return the timer for scheduling
		// when to engage dive mode
		if (m_dive_state == DiveState::StartPending) {
			DEBUG_INFO("DiveModeService: dive mode start pending");
			return service_read_param<unsigned int>(ParamID::UW_DIVE_MODE_START_TIME) * 1000;
		} else {
			return SCHEDULE_DISABLED;
		}
	}

	bool service_is_enabled() override {
		return service_read_param<bool>(ParamID::UW_DIVE_MODE_ENABLE);
	}

	bool service_is_usable_underwater() override { return true; }

	void service_init() override {
		m_dive_state = DiveState::Idle;
		if (service_is_enabled()) {
			m_wchg.enable([this]() {
				if (m_dive_state == DiveState::Engaged) {
					DEBUG_INFO("DiveModeService: dive mode disengaged");
					m_dive_state = DiveState::Disengaged;
					m_reed.resume();
				}
			});
		}
	}

	void service_term() override {
		if (service_is_enabled()) {
			m_wchg.disable();
			m_reed.resume();
		}
	}
};
