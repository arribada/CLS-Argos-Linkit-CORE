#include "uwdetector_service.hpp"

#include "debug.hpp"


void UWDetectorService::service_initiate() {

	bool new_state;
	DEBUG_TRACE("UWDetector::sample_sws: m_sample_iteration=%u", m_sample_iteration);

	// Sample the switch
	new_state = detector_state();

	DEBUG_TRACE("UWDetector::sample_detector state=%u", new_state);

	if (new_state) {

		m_sample_iteration++;

	} else {

		m_sample_iteration = m_iterations;  // Terminate early

	}

	if (m_sample_iteration >= m_iterations) {

		// If we reached the terminal number of iterations then the SWS state must have been
		// set for all sampling iterations -- we treat SWS as definitively set.

		DEBUG_TRACE("UWDetector::sample_detector: terminal iterations reached: state=%u", new_state);

		m_sample_iteration = 0;

		if (new_state != m_current_state || m_is_first_time) {
			DEBUG_INFO("UWDetector::sample_detector: state changed: state=%u", (unsigned int)new_state);
			m_is_first_time = false;
			m_current_state = new_state;
			ServiceEventData event = new_state;
			service_complete(&event);
		} else {
			DEBUG_TRACE("UWDetector::sample_detector: state unchanged");
			service_complete();
		}
	} else {
		service_complete();
	}
}

void UWDetectorService::service_init() {
	m_is_first_time = true;
	m_period_underwater_ms = m_sched_units * 1000 * service_read_param<unsigned int>(ParamID::SAMPLING_UNDER_FREQ);
	m_period_surface_ms = m_sched_units * 1000 * service_read_param<unsigned int>(ParamID::SAMPLING_SURF_FREQ);
	m_activation_threshold = service_read_param<double>(ParamID::UNDERWATER_DETECT_THRESH);
	m_sample_iteration = 0;
}

void UWDetectorService::service_term() {
};

unsigned int UWDetectorService::service_next_schedule_in_ms() {
	if (m_sample_iteration)
		return m_period_ms;
	else
		return m_current_state ? m_period_underwater_ms : m_period_surface_ms;
}

bool UWDetectorService::service_is_usable_underwater() {
	return true;
}
