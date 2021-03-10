#include "bsp.hpp"
#include "sws.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"
#include "gpio.hpp"
#include "debug.hpp"
#include "pmu.hpp"

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;


#define SAMPLING_ITERATIONS	  5
#define SAMPLING_PERIOD_MS	  1000

void SWS::sample_sws() {

	bool new_state;
	DEBUG_TRACE("SWS::sample_sws: m_sample_iteration=%u", m_sample_iteration);

	// Sample the SWS pin
	GPIOPins::set(BSP::GPIO::GPIO_SLOW_SWS_SEND);
	PMU::delay_ms(1); // Wait a while to allow for any water capacitance (this value is a total guess)
	new_state = GPIOPins::value(BSP::GPIO::GPIO_SLOW_SWS_RX);
	GPIOPins::clear(BSP::GPIO::GPIO_SLOW_SWS_SEND);

	DEBUG_TRACE("SWS::sample_sws: state=%u", new_state);

	if (new_state) {

		m_sample_iteration++;

	} else {

		m_sample_iteration = SAMPLING_ITERATIONS;  // Terminate early

	}

	if (m_sample_iteration >= SAMPLING_ITERATIONS) {

		// If we reached the terminal number of iterations then the SWS state must have been
		// set for all sampling iterations -- we treat SWS as definitively set.

		DEBUG_TRACE("SWS::sample_sws: terminal iterations reached: state=%u", new_state);

		m_sample_iteration = 0;

		if (new_state != m_current_state || m_is_first_time) {
			DEBUG_TRACE("SWS::sample_sws: notifying caller");
			m_is_first_time = false;
			m_current_state = new_state;
			m_state_change_handler(m_current_state);
		} else {
			DEBUG_TRACE("SWS::sample_sws: not notifying caller - state unchanged");
		}

		// Activate next schedule
		m_sample_iteration = 0;
		unsigned int new_sched = new_state ? m_period_underwater_ms : m_period_surface_ms;
		DEBUG_TRACE("SWS::sample_sws: new_sched=%u", new_sched);
		m_task_handle = system_scheduler->post_task_prio(std::bind(&SWS::sample_sws, this), Scheduler::DEFAULT_PRIORITY, new_sched);

	} else {
		DEBUG_TRACE("SWS::sample_sws: new_sched=%u", SAMPLING_PERIOD_MS);
		m_task_handle = system_scheduler->post_task_prio(std::bind(&SWS::sample_sws, this), Scheduler::DEFAULT_PRIORITY, SAMPLING_PERIOD_MS);
	}
}

SWS::~SWS() {
	stop();
}

void SWS::start(std::function<void(bool)> func) {
	Switch::start(func);
	m_is_first_time = true;
	m_period_underwater_ms = m_sched_units * 1000 * configuration_store->read_param<unsigned int>(ParamID::SAMPLING_UNDER_FREQ);
	m_period_surface_ms = m_sched_units * 1000 * configuration_store->read_param<unsigned int>(ParamID::SAMPLING_SURF_FREQ);
	m_sample_iteration = 0;
	bool is_underwater_enabled = configuration_store->read_param<bool>(ParamID::UNDERWATER_EN);
	// Do not schedule first sampling unless UNDERWATER_EN is set
	if (is_underwater_enabled)
		m_task_handle = system_scheduler->post_task_prio(std::bind(&SWS::sample_sws, this));
}

void SWS::stop() {
	Switch::stop();
	system_scheduler->cancel_task(m_task_handle);
}
