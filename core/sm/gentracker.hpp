#ifndef __GENTRACKER_HPP_INC
#define __GENTRACKER_HPP_INC

#include "tinyfsm.hpp"
#include "system_log.hpp"
#include "sensor_log.hpp"
#include "config_store.hpp"
#include "gps_scheduler.hpp"
#include "argos_scheduler.hpp"
#include "scheduler.hpp"
#include "ota_update_service.hpp"
#include "dte_service.hpp"
#include "filesystem.hpp"
#include "config_store.hpp"
#include "error.hpp"

// These contexts must be created before the FSM is initialised
extern Scheduler  *main_sched;
extern FileSystem *main_filesystem;

struct ReedSwitchEvent              : tinyfsm::Event { bool state; };
struct SaltwaterSwitchEvent         : tinyfsm::Event { bool state; };
struct ConfigurationStatusEvent     : tinyfsm::Event { bool is_valid; };
struct ErrorEvent : tinyfsm::Event { ErrorCode error_code; };


class GenTracker : public tinyfsm::Fsm<GenTracker>
{
private:
	static SensorLog *m_sensor_log;
	static SystemLog *m_system_log;
	static ConfigurationStore *m_config_store;
	static GPSScheduler *m_gps_sched;
	static ArgosScheduler *m_argos_sched;
	static DTEService *m_dte_serv;
	static OTAUpdateService *m_ota_update_serv;
	static ErrorCode last_error;

	void notify_config_store_state() {
		ConfigurationStatusEvent event;
		event.is_valid = m_config_store->is_valid();
		dispatch(event);
	}

	void notify_bad_filesystem_error() {
		ErrorEvent event;
		event.error_code = ERROR_BAD_FILESYSTEM;
		dispatch(event);
	}

public:
	void react(tinyfsm::Event const &) { };
	void react(ReedSwitchEvent const &event)
	{
		if (event.state)
		{
			transit<ConfigurationState>();
		}
	};
	void react(SaltwaterSwitchEvent const &event) {
		m_config_store->notify_saltwater_switch_state(event.state);
	};
	void react(ConfigurationStatusEvent const &) { };
	void react(ErrorEvent const &event) {
		last_error = event.error_code;
		transit<ErrorState>();
	};

	void entry(void) { };
	void exit(void)  { };


};


class BootState : GenTracker
{
	void enter() {

		main_sched->register_task(notify_bad_filesystem_error);
		main_sched->register_task(notify_config_store_state);

		// If we can't mount the filesystem then try to format it first and retry
		if (main_filesystem->mount() < 0)
		{
			main_filesystem->format();
			if (main_filesystem->mount() < 0)
			{
				// We can't mount a formatted filesystem, something bad has happened
				main_sched->post_task_prio(notify_bad_filesystem_error);
				return;
			}
		}

		try {
			// The underlying classes will create the files on the filesystem if they do not
			// already yet exist
			m_sensor_log = new SensorLog(main_filesystem);
			m_system_log = new SystemLog(main_filesystem);
			m_config_store = new ConfigurationStore(main_filesystem);
			main_sched->post_task_prio(notify_config_store_state);
		} catch (int e) {
			main_sched->post_task_prio(notify_bad_filesystem_error);
		}
	}

	void react(ConfigurationStatusEvent const &event)
	{
		if (event.is_valid) {
			transit<OperationalState>();
		}
	};
};

class OperationalState : GenTracker
{
	void react(SaltwaterSwitchEvent const &event)
	{
		m_config_store->notify_saltwater_switch_state(event.state);
		m_gps_sched->notify_saltwater_switch_state(event.state);
		m_argos_sched->notify_saltwater_switch_state(event.state);
	};

	void enter() {
		m_gps_sched = GPSScheduler(m_config_store, m_sensor_log, m_system_log);
		m_argos_sched = ArgosScheduler(m_config_store, m_sensor_log, m_system_log);
	}

	void exit() {
		delete m_gps_sched;
		delete m_argos_sched;
	}

};

class ConfigurationState : GenTracker
{
	void react(ReedSwitchEvent const &event)
	{
		if (!event.state)
			transit<ConfigurationState>();
	};

	void enter() {
		m_dte_serv = new DTEService([] {
		},
		[]() {
			// After a DTE disconnection, re-evaluate if the configuration store
			// is valid and notify all event listeners
			main_sched->post_task_prio(notify_config_store_state);
		});
		m_ota_update_serv = new OTAUpdateService([]{}, []{});
	}

	void exit() {
		delete m_dte_serv;
		delete m_ota_update_serv;
	}
};

class ErrorState : GenTracker
{
	void react(ReedSwitchEvent const &event)
	{
		if (event.state)
		{
			transit<ConfigurationState>();
		}
	};

	void reset() {
		main_sched->register_task(error_task);
	}

	void enter() {
		main_sched->post_task_prio(error_task);
	}

	void exit() {
		main_sched->cancel_task(error_task);
	}

	void error_task() {
		// TODO: handle LED error indication display on LEDs

		// Invoke the scheduler to call us again in 1 second
		main_sched->post_task_prio(error_task, Scheduler::DEFAULT_PRIORITY, 1000);
	}
};


#endif // __GENTRACKER_HPP_INC
