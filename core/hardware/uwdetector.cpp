#include "bsp.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"
#include "gpio.hpp"
#include "debug.hpp"
#include "pmu.hpp"
#include "uwdetector.hpp"

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;


void UWDetector::sample_detector() {

	bool new_state;
	DEBUG_TRACE("SWS::sample_sws: m_sample_iteration=%u", m_sample_iteration);

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
			DEBUG_TRACE("UWDetector::sample_detector: notifying caller");
			m_is_first_time = false;
			m_current_state = new_state;
			m_state_change_handler(m_current_state);
		} else {
			DEBUG_TRACE("UWDetector::sample_detector: not notifying caller - state unchanged");
		}

		// Activate next schedule
		m_sample_iteration = 0;
		unsigned int new_sched = new_state ? m_period_underwater_ms : m_period_surface_ms;
		DEBUG_TRACE("UWDetector::sample_detector: new_sched=%u", new_sched);
		m_task_handle = system_scheduler->post_task_prio(std::bind(&UWDetector::sample_detector, this),
				"UWSampleSwitch",
				Scheduler::DEFAULT_PRIORITY, new_sched);

	} else {
		DEBUG_TRACE("SWS::sample_sws: new_sched=%u", m_period_ms);
		m_task_handle = system_scheduler->post_task_prio(std::bind(&UWDetector::sample_detector, this),
				"UWSampleSwitch",
				Scheduler::DEFAULT_PRIORITY, m_period_ms);
	}
}

UWDetector::~UWDetector() {
	stop();
}

void UWDetector::start(std::function<void(bool)> func) {
	Switch::start(func);
	m_is_first_time = true;
	m_period_underwater_ms = m_sched_units * 1000 * configuration_store->read_param<unsigned int>(ParamID::SAMPLING_UNDER_FREQ);
	m_period_surface_ms = m_sched_units * 1000 * configuration_store->read_param<unsigned int>(ParamID::SAMPLING_SURF_FREQ);
	m_sample_iteration = 0;
	m_activation_threshold = configuration_store->read_param<double>(ParamID::UNDERWATER_DETECT_THRESH);
	bool is_underwater_enabled = configuration_store->read_param<bool>(ParamID::UNDERWATER_EN);
	// Do not schedule first sampling unless UNDERWATER_EN is set
	if (is_underwater_enabled)
		m_task_handle = system_scheduler->post_task_prio(std::bind(&UWDetector::sample_detector, this), "UWSampleSwitch");
}

void UWDetector::stop() {
	Switch::stop();
	system_scheduler->cancel_task(m_task_handle);
}
