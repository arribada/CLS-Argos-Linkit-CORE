#include "uwdetector_service.hpp"

#include "debug.hpp"


void UWDetectorService::service_initiate() {

	bool new_state;
	DEBUG_TRACE("UWDetectorService: m_sample_iteration=%u dry=%u", m_sample_iteration, m_dry_count);

	// Sample the switch
	new_state = detector_state();
	m_sample_iteration++;

	DEBUG_TRACE("UWDetectorService: state=%u", new_state);

	if (new_state) {
		m_pending_state = true;
	} else {
		if (++m_dry_count >= m_min_dry_samples) {
			DEBUG_TRACE("UWDetectorService: terminate early as dry=%u", m_dry_count);
			m_sample_iteration = m_max_samples;  // Terminate early
			m_pending_state = false;
		}
	}

	if (m_sample_iteration >= m_max_samples) {

		// If we reached the terminal number of iterations then the SWS state must have been
		// set for all sampling iterations -- we treat SWS as definitively set.

		DEBUG_TRACE("UWDetectorService: terminal iterations reached: state=%u", m_pending_state);

		m_sample_iteration = 0;
		m_dry_count = 0;

		if (m_pending_state != m_current_state || m_is_first_time) {
			DEBUG_INFO("UWDetectorService: state changed: state=%u", (unsigned int)m_pending_state);
			m_is_first_time = false;
			m_current_state = m_pending_state;
			ServiceEventData event = m_pending_state;
			service_complete(&event);
		} else {
			DEBUG_TRACE("UWDetectorService state unchanged");
			service_complete();
		}
		m_pending_state = false;
	} else {
		service_complete();
	}
}

void UWDetectorService::service_init() {
	m_is_first_time = true;
	m_period_underwater_ms = 1000 * service_read_param<unsigned int>(ParamID::SAMPLING_UNDER_FREQ);
	m_period_surface_ms = 1000 * service_read_param<unsigned int>(ParamID::SAMPLING_SURF_FREQ);
	m_activation_threshold = service_read_param<double>(ParamID::UNDERWATER_DETECT_THRESH);
	m_sample_gap = service_read_param<unsigned int>(ParamID::UW_SAMPLE_GAP);
	m_enable_sample_delay = service_read_param<unsigned int>(ParamID::UW_PIN_SAMPLE_DELAY);
	m_sample_iteration = 0;
	m_dry_count = 0;
	m_pending_state = false;
	BaseUnderwaterDetectSource src = service_read_param<BaseUnderwaterDetectSource>(ParamID::UNDERWATER_DETECT_SOURCE);
	if (src == BaseUnderwaterDetectSource::SWS) {
		m_max_samples = service_read_param<unsigned int>(ParamID::UW_MAX_SAMPLES);
		m_min_dry_samples = service_read_param<unsigned int>(ParamID::UW_MIN_DRY_SAMPLES);
	} else {
		m_max_samples = 1;
		m_min_dry_samples = 1;
	}
}

void UWDetectorService::service_term() {
};

unsigned int UWDetectorService::service_next_schedule_in_ms() {
	if (m_sample_iteration)
		return m_sample_gap;
	else
		return m_is_first_time ? 0 : (m_current_state ? m_period_underwater_ms : m_period_surface_ms);
}

bool UWDetectorService::service_is_usable_underwater() {
	return true;
}
