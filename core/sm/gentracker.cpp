#include "bsp.hpp"
#include "pmu.hpp"
#include "ota_file_updater.hpp"
#include "logger.hpp"
#include "config_store.hpp"
#include "location_scheduler.hpp"
#include "comms_scheduler.hpp"
#include "scheduler.hpp"
#include "dte_handler.hpp"
#include "filesystem.hpp"
#include "config_store.hpp"
#include "error.hpp"
#include "timer.hpp"
#include "debug.hpp"
#include "switch.hpp"
#include "rgb_led.hpp"
#include "battery.hpp"
#include "ble_service.hpp"
#include "gentracker.hpp"

// These contexts must be created before the FSM is initialised
extern FileSystem *main_filesystem;
extern Scheduler *system_scheduler;
extern Timer *system_timer;
extern CommsScheduler *comms_scheduler;
extern LocationScheduler *location_scheduler;
extern Logger *sensor_log;
extern Logger *system_log;
extern ConfigurationStore *configuration_store;
extern BLEService *ble_service;
extern OTAFileUpdater *ota_updater;
extern DTEHandler *dte_handler;
extern Switch *saltwater_switch;
extern Switch *reed_switch;
extern RGBLed *status_led;
extern BatteryMonitor *battery_monitor;


void GenTracker::react(tinyfsm::Event const &) { }

void GenTracker::react(ReedSwitchEvent const &event)
{
	DEBUG_TRACE("react: ReedSwitchEvent: %u", event.state);
	if (event.state) {
		// Start reed switch hold timer tasks in case this is a hold gesture
		m_reed_trigger_start_time = system_timer->get_counter();
		m_task_trigger_config_state = system_scheduler->post_task_prio([this](){ if (!is_in_state<ConfigurationState>()) transit<ConfigurationState>(); }, Scheduler::DEFAULT_PRIORITY, TRANSIT_CONFIG_HOLD_TIME_MS);
		m_task_trigger_off_state = system_scheduler->post_task_prio([this](){ transit<OffState>(); }, Scheduler::DEFAULT_PRIORITY, TRANSIT_OFF_HOLD_TIME_MS);
	} else {
		// Cancel any pending hold gestures
		system_scheduler->cancel_task(m_task_trigger_config_state);
		system_scheduler->cancel_task(m_task_trigger_off_state);
	}
}

void GenTracker::react(SaltwaterSwitchEvent const &) { }

void GenTracker::react(ErrorEvent const &event) {
	(void)event;
	transit<ErrorState>();
}

void GenTracker::entry(void) { };
void GenTracker::exit(void)  { };

void GenTracker::notify_bad_filesystem_error() {
	ErrorEvent event;
	event.error_code = BAD_FILESYSTEM;
	dispatch(event);
}

void BootState::entry() {

	DEBUG_TRACE("entry: BootState");

	// Ensure the system timer is started to allow scheduling to work
	system_timer->start();

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

	// Start reed switch monitoring and dispatch events to state machine
	reed_switch->start([](bool s) { ReedSwitchEvent e; e.state = s; dispatch(e); });

	// Start battery monitor
	battery_monitor->start();

	// Turn status LED white to indicate boot up
	status_led->set(RGBLedColor::WHITE);

	try {
		// The underlying classes will create the files on the filesystem if they do not
		// already yet exist
		sensor_log->create();
		system_log->create();
		configuration_store->init();
		// Transition to IDLE state after initialisation
		system_scheduler->post_task_prio([this](){
			transit<IdleState>();
		},
		Scheduler::DEFAULT_PRIORITY,
		1000);
	} catch (int e) {
		system_scheduler->post_task_prio(notify_bad_filesystem_error);
	}
}

void BootState::exit() {

	DEBUG_TRACE("exit: BootState");

	// Turn status LED off to indicate exit from boot state
	status_led->off();
}

void OffState::entry() {
	DEBUG_TRACE("entry: OffState");
	battery_monitor->stop();
	status_led->flash(RGBLedColor::WHITE, 125);  // Flash 4X speed during power down
	system_scheduler->post_task_prio([](){ status_led->off(); PMU::powerdown(); }, Scheduler::DEFAULT_PRIORITY, OFF_LED_PERIOD_MS);
}

void OffState::exit() {
	// !!! This state should never be entered !!!
	DEBUG_TRACE("exit: OffState");
}

void IdleState::entry() {
	DEBUG_TRACE("entry: IdleState");
	if (configuration_store->is_valid()) {
		if (configuration_store->is_battery_level_low())
			status_led->set(RGBLedColor::YELLOW);
		else
			status_led->set(RGBLedColor::GREEN);
		m_idle_state_task = system_scheduler->post_task_prio([this](){ transit<OperationalState>(); }, Scheduler::DEFAULT_PRIORITY, IDLE_PERIOD_MS);
	} else {
		status_led->set(RGBLedColor::RED);
		m_idle_state_task = system_scheduler->post_task_prio([this](){ transit<ErrorState>(); }, Scheduler::DEFAULT_PRIORITY, IDLE_PERIOD_MS);
	}
}

void IdleState::exit() {
	DEBUG_TRACE("exit: IdleState");
	system_scheduler->cancel_task(m_idle_state_task);
	status_led->off();
}

void OperationalState::react(SaltwaterSwitchEvent const &event)
{
	DEBUG_TRACE("react: SaltwaterSwitchEvent");
	configuration_store->notify_saltwater_switch_state(event.state);
	location_scheduler->notify_saltwater_switch_state(event.state);
	comms_scheduler->notify_saltwater_switch_state(event.state);
};

void OperationalState::entry() {
	DEBUG_TRACE("entry: OperationalState");
	if (configuration_store->is_battery_level_low())
		status_led->flash(RGBLedColor::YELLOW);
	else
		status_led->flash(RGBLedColor::GREEN);
	system_scheduler->post_task_prio([](){ status_led->off(); }, Scheduler::DEFAULT_PRIORITY, LED_INDICATION_PERIOD_MS);
	saltwater_switch->start([](bool s) { SaltwaterSwitchEvent e; e.state = s; dispatch(e); });
	comms_scheduler->start();
	location_scheduler->start([]() { comms_scheduler->notify_sensor_log_update(); });
}

void OperationalState::exit() {
	DEBUG_TRACE("exit: OperationalState");
	status_led->off();
	location_scheduler->stop();
	comms_scheduler->stop();
	saltwater_switch->stop();
}

void ConfigurationState::entry() {
	DEBUG_TRACE("entry: ConfigurationState");

	// Flash the blue LED to indicate we have started BLE and we are
	// waiting for a connection
	status_led->flash(RGBLedColor::BLUE);

	ble_service->start([this](BLEServiceEvent& event) -> int { return on_ble_event(event); } );
	restart_inactivity_timeout();
}

void ConfigurationState::exit() {
	DEBUG_TRACE("exit: ConfigurationState");
	system_scheduler->cancel_task(m_ble_inactivity_timeout_task);
	ble_service->stop();
	status_led->off();
}

int ConfigurationState::on_ble_event(BLEServiceEvent& event) {
	int rc = 0;

	switch (event.event_type) {
	case BLEServiceEventType::CONNECTED:
		DEBUG_INFO("ConfigurationState::on_ble_event: CONNECTED");
		// Indicate DTE connection is made
		status_led->set(RGBLedColor::BLUE);
		restart_inactivity_timeout();
		break;
	case BLEServiceEventType::DISCONNECTED:
		DEBUG_INFO("ConfigurationState::on_ble_event: DISCONNECTED");
		status_led->flash(RGBLedColor::BLUE);
		break;
	case BLEServiceEventType::DTE_DATA_RECEIVED:
		DEBUG_TRACE("ConfigurationState::on_ble_event: DTE_DATA_RECEIVED");
		restart_inactivity_timeout();
		system_scheduler->post_task_prio(std::bind(&ConfigurationState::process_received_data, this));
		break;
	case BLEServiceEventType::OTA_START:
		DEBUG_TRACE("ConfigurationState::on_ble_event: OTA_START");
		restart_inactivity_timeout();
		ota_updater->start_file_transfer((OTAFileIdentifier)event.file_id, event.file_size, event.crc32);
		break;
	case BLEServiceEventType::OTA_END:
		DEBUG_TRACE("ConfigurationState::on_ble_event: OTA_END");
		restart_inactivity_timeout();
		ota_updater->complete_file_transfer();
		system_scheduler->post_task_prio(std::bind(&OTAFileUpdater::apply_file_update, ota_updater));
		break;
	case BLEServiceEventType::OTA_ABORT:
		DEBUG_TRACE("ConfigurationState::on_ble_event: OTA_ABORT");
		restart_inactivity_timeout();
		ota_updater->abort_file_transfer();
		break;
	case BLEServiceEventType::OTA_FILE_DATA:
		DEBUG_TRACE("ConfigurationState::on_ble_event: OTA_FILE_DATA");
		restart_inactivity_timeout();
		ota_updater->write_file_data(event.data, event.length);
		break;
	default:
		break;
	}

	return rc;
}


void ConfigurationState::on_ble_inactivity_timeout() {
	DEBUG_INFO("BLE Inactivity Timeout");
	transit<OffState>();
}

void ConfigurationState::restart_inactivity_timeout() {
	DEBUG_TRACE("Restart BLE inactivity timeout: %lu", system_timer->get_counter());
	system_scheduler->cancel_task(m_ble_inactivity_timeout_task);
	m_ble_inactivity_timeout_task = system_scheduler->post_task_prio(std::bind(&ConfigurationState::on_ble_inactivity_timeout, this), Scheduler::DEFAULT_PRIORITY, BLE_INACTIVITY_TIMEOUT_MS);
}

void ConfigurationState::process_received_data() {
	auto req = ble_service->read_line();

	if (req.size())
	{
		DEBUG_TRACE("received %u bytes:", req.size());
		printf("%s\n", req.c_str());

		std::string resp;
		DTEAction action;

		do
		{
			action = dte_handler->handle_dte_message(req, resp);
			if (resp.size())
			{
				DEBUG_TRACE("responded: %s", resp.c_str());
				ble_service->write(resp);
			}

			if (action == DTEAction::FACTR)
			{
				DEBUG_TRACE("Perform factory reset of configuration store");
				configuration_store->factory_reset();

				// After formatting the filesystem we must do a system reset so that
				// the boot up procedure can repopulate files
				PMU::reset();
			}
			else if (action == DTEAction::RESET)
			{
				DEBUG_TRACE("Perform device reset");

				// Execute this after 3 seconds to allow time for the BLE response to be sent
				system_scheduler->post_task_prio([](){ PMU::reset(); }, Scheduler::DEFAULT_PRIORITY, 3000);
			}
			else if (action == DTEAction::SECUR)
			{
				// TODO: add secure procedure
				DEBUG_TRACE("Perform secure procedure");
			}

		} while (action == DTEAction::AGAIN);
	}
}

void ErrorState::entry() {
	DEBUG_TRACE("entry: ErrorState");
	status_led->flash(RGBLedColor::RED);
	system_scheduler->post_task_prio([this](){ transit<OffState>(); }, Scheduler::DEFAULT_PRIORITY, 5000);
}

void ErrorState::exit() {
	DEBUG_TRACE("exit: ErrorState");
	status_led->off();
}
