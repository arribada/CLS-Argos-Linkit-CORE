#ifndef __GENTRACKER_HPP_INC
#define __GENTRACKER_HPP_INC

#include "tinyfsm.hpp"
#include "logger.hpp"
#include "config_store.hpp"
#include "gps_scheduler.hpp"
#include "comms_scheduler.hpp"
#include "scheduler.hpp"
#include "ota_update_service.hpp"
#include "dte_service.hpp"
#include "filesystem.hpp"
#include "config_store.hpp"
#include "error.hpp"
#include "timer.hpp"
#include "debug.hpp"
#include "switch.hpp"
#include "led.hpp"

// These contexts must be created before the FSM is initialised
extern FileSystem *main_filesystem;
extern Scheduler *system_scheduler;
extern Timer *system_timer;
extern CommsScheduler *comms_scheduler;
extern GPSScheduler *gps_scheduler;
extern Logger *sensor_log;
extern Logger *system_log;
extern ConfigurationStore *configuration_store;
extern BLEService *dte_service;
extern BLEService *ota_update_service;
extern Switch *saltwater_switch;
extern Switch *reed_switch;
extern Led *red_led;
extern Led *green_led;
extern Led *blue_led;

struct ReedSwitchEvent              : tinyfsm::Event { bool state; };
struct SaltwaterSwitchEvent         : tinyfsm::Event { bool state; };
struct ConfigurationStatusEvent     : tinyfsm::Event { bool is_valid; };
struct ErrorEvent : tinyfsm::Event { ErrorCode error_code; };

class ConfigurationState;
class ErrorState;
class OperationalState;

class GenTracker : public tinyfsm::Fsm<GenTracker>
{
public:
	ErrorCode last_error;

	void react(tinyfsm::Event const &) { };
	void react(ReedSwitchEvent const &event)
	{
		DEBUG_TRACE("react: ReedSwitchEvent");
		if (event.state)
		{
			transit<ConfigurationState>();
		}
	};
	virtual void react(SaltwaterSwitchEvent const &event) { };
	virtual void react(ConfigurationStatusEvent const &) { };
	void react(ErrorEvent const &event) {
		last_error = event.error_code;
		transit<ErrorState>();
	};

	virtual void entry(void) { };
	virtual void exit(void)  { };

	static void notify_config_store_state() {
		ConfigurationStatusEvent event;
		event.is_valid = configuration_store->is_valid();
		dispatch(event);
	}

	static void notify_bad_filesystem_error() {
		ErrorEvent event;
		event.error_code = BAD_FILESYSTEM;
		dispatch(event);
	}

};


class BootState : public GenTracker
{
	void entry() override {

		DEBUG_TRACE("entry: BootState");

		// If we can't mount the filesystem then try to format it first and retry
		DEBUG_TRACE("mount filesystem");
		if (main_filesystem->mount() < 0)
		{
			DEBUG_TRACE("format filesystem");
			if (main_filesystem->format() < 0 || main_filesystem->mount() < 0)
			{
				// We can't mount a formatted filesystem, something bad has happened
				system_scheduler->post_task_prio(notify_bad_filesystem_error);
				return;
			}
		}

		// Ensure the system timer is started to allow scheduling to work
		system_timer->start();

		// Start both the saltwater and reed switch monitoring
		reed_switch->start([](bool state) { ReedSwitchEvent e; e.state = state; dispatch(e); });
		saltwater_switch->start([](bool state) { SaltwaterSwitchEvent e; e.state = state; dispatch(e); });

		// Turn off all LEDs
		red_led->off();
		green_led->off();
		blue_led->off();

		try {
			// The underlying classes will create the files on the filesystem if they do not
			// already yet exist
			sensor_log->create();
			system_log->create();
			configuration_store->init();
			system_scheduler->post_task_prio(notify_config_store_state);
		} catch (int e) {
			system_scheduler->post_task_prio(notify_bad_filesystem_error);
		}
	}

	void react(SaltwaterSwitchEvent const &event)
	{
		DEBUG_TRACE("react: SaltwaterSwitchEvent");
		configuration_store->notify_saltwater_switch_state(event.state);
	};

	void react(ConfigurationStatusEvent const &event)
	{
		DEBUG_TRACE("react: ConfigurationStatusEvent");
		if (event.is_valid) {
			transit<OperationalState>();
		}
	};
};

class OperationalState : public GenTracker
{
	void react(SaltwaterSwitchEvent const &event)
	{
		DEBUG_TRACE("react: SaltwaterSwitchEvent");
		configuration_store->notify_saltwater_switch_state(event.state);
		gps_scheduler->notify_saltwater_switch_state(event.state);
		comms_scheduler->notify_saltwater_switch_state(event.state);
	};

	void entry() {
		DEBUG_TRACE("entry: OperationalState");
		// Flash the green LED for 5 seconds
		green_led->flash();
		system_scheduler->post_task_prio([](){green_led->off();}, Scheduler::DEFAULT_PRIORITY, 5000);

		gps_scheduler->start();
		comms_scheduler->start();
	}

	void exit() {
		DEBUG_TRACE("exit: OperationalState");
		green_led->off();
		gps_scheduler->stop();
		comms_scheduler->stop();
	}
};

class ConfigurationState : public GenTracker
{
	void react(SaltwaterSwitchEvent const &event)
	{
		DEBUG_TRACE("react: SaltwaterSwitchEvent");
		configuration_store->notify_saltwater_switch_state(event.state);
	};

	void react(ReedSwitchEvent const &event)
	{
		DEBUG_TRACE("react: ReedSwitchEvent");
		if (!event.state) {
			system_scheduler->post_task_prio(notify_config_store_state);
		}
	};

	void entry() {
		DEBUG_TRACE("entry: ConfigurationState");

		// Flash the blue LED to indicate we have started BLE and we are
		// waiting for a connection
		blue_led->flash();

		dte_service->start([] {
			// Indicate DTE connection is made
			blue_led->on();
		},
		[]() {
			// After a DTE disconnection, re-evaluate if the configuration store
			// is valid and notify all event listeners
			system_scheduler->post_task_prio(notify_config_store_state);
			blue_led->off();
		});
		ota_update_service->start([]{}, []{});
	}

	void exit() {
		DEBUG_TRACE("exit: ConfigurationState");
		dte_service->stop();
		ota_update_service->stop();
		blue_led->off();
	}
};

class ErrorState : public GenTracker
{
	void react(SaltwaterSwitchEvent const &event)
	{
		DEBUG_TRACE("react: SaltwaterSwitchEvent");
		configuration_store->notify_saltwater_switch_state(event.state);
	};

	void react(ReedSwitchEvent const &event)
	{
		DEBUG_TRACE("react: ReedSwitchEvent");
		if (event.state)
		{
			transit<ConfigurationState>();
		}
	};

	void entry() {
		DEBUG_TRACE("entry: ErrorState");
		// Call the error task immediately
		error_task_handle = system_scheduler->post_task_prio(error_task);
	}

	void exit() {
		DEBUG_TRACE("exit: ErrorState");
		// Cancel any pending calls
		system_scheduler->cancel_task(error_task_handle);
	}

	static void error_task() {
		// TODO: handle LED error indication display on error LED

		// Invoke the scheduler to call us again in 1 second
		system_scheduler->post_task_prio(error_task, Scheduler::DEFAULT_PRIORITY, 1000);
	}

private:
	Scheduler::TaskHandle error_task_handle;
};


#endif // __GENTRACKER_HPP_INC
