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
#include "timer.hpp"

// These contexts must be created before the FSM is initialised
extern FileSystem *main_filesystem;

struct ReedSwitchEvent              : tinyfsm::Event { bool state; };
struct SaltwaterSwitchEvent         : tinyfsm::Event { bool state; };
struct ConfigurationStatusEvent     : tinyfsm::Event { bool is_valid; };
struct ErrorEvent : tinyfsm::Event { ErrorCode error_code; };

// Class prototypes
class BootState;
class OperationalState;
class ConfigurationState;
class ErrorState;

class GenTracker : public tinyfsm::Fsm<GenTracker>
{
private:
	static ErrorCode last_error;

	void notify_config_store_state() {
		ConfigurationStatusEvent event;
		event.is_valid = ConfigurationStore::is_valid();
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
		ConfigurationStore::notify_saltwater_switch_state(event.state);
	};
	void react(ConfigurationStatusEvent const &) { };
	void react(ErrorEvent const &event) {
		last_error = event.error_code;
		transit<ErrorState>();
	};

	void entry(void) { };
	void exit(void)  { };


};


class BootState : public GenTracker
{
	void enter() {

		// Ensure the system timer is started before anything
		Timer::start();

		// If we can't mount the filesystem then try to format it first and retry
		if (main_filesystem->mount() < 0)
		{
			main_filesystem->format();
			if (main_filesystem->mount() < 0)
			{
				// We can't mount a formatted filesystem, something bad has happened
				Scheduler::post_task_prio(notify_bad_filesystem_error);
				return;
			}
		}

		try {
			// The underlying classes will create the files on the filesystem if they do not
			// already yet exist
			SensorLog::create();
			SystemLog::create();
			ConfigurationStore::init();
			Scheduler::post_task_prio(notify_config_store_state);
		} catch (int e) {
			Scheduler::post_task_prio(notify_bad_filesystem_error);
		}
	}

	void react(ConfigurationStatusEvent const &event)
	{
		if (event.is_valid) {
			transit<OperationalState>();
		}
	};
};

class OperationalState : public GenTracker
{
	void react(SaltwaterSwitchEvent const &event)
	{
		ConfigurationStore::notify_saltwater_switch_state(event.state);
		GPSScheduler::notify_saltwater_switch_state(event.state);
		ArgosScheduler::notify_saltwater_switch_state(event.state);
	};

	void enter() {
		GPSScheduler::start();
		ArgosScheduler::start();
	}

	void exit() {
		GPSScheduler::stop();
		ArgosScheduler::stop();
	}

};

class ConfigurationState : public GenTracker
{
	void react(ReedSwitchEvent const &event)
	{
		if (!event.state)
			transit<ConfigurationState>();
	};

	void enter() {
		DTEService::start([] {
		},
		[]() {
			// After a DTE disconnection, re-evaluate if the configuration store
			// is valid and notify all event listeners
			Scheduler::post_task_prio(notify_config_store_state);
		});
		OTAUpdateService::start([]{}, []{});
	}

	void exit() {
		DTEService::stop();
		OTAUpdateService::stop();
	}
};

class ErrorState : public GenTracker
{
	void react(ReedSwitchEvent const &event)
	{
		if (event.state)
		{
			transit<ConfigurationState>();
		}
	};

	void enter() {
		// Call the error task immediately
		Scheduler::post_task_prio(error_task);
	}

	void exit() {
		// Cancel any pending calls
		Scheduler::cancel_task(error_task);
	}

	void error_task() {
		// TODO: handle LED error indication display on LEDs

		// Invoke the scheduler to call us again in 1 second
		Scheduler::post_task_prio(error_task, Scheduler::DEFAULT_PRIORITY, 1000);
	}
};


#endif // __GENTRACKER_HPP_INC
