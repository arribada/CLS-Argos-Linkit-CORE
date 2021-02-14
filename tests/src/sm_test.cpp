#include "gentracker.hpp"
#include "debug.hpp"

#include "fake_switch.hpp"
#include "fake_rgb_led.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_ble_serv.hpp"
#include "mock_comms.hpp"
#include "mock_config_store.hpp"
#include "mock_fs.hpp"
#include "mock_location.hpp"
#include "mock_logger.hpp"
#include "mock_timer.hpp"
#include "mock_battery_mon.hpp"
#include "mock_ota_file_updater.hpp"
#include "scheduler.hpp"
#include "dte_handler.hpp"
#include "linux_timer.hpp"
#include "bsp.hpp"

#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)

// These are defined by main.cpp
extern FileSystem *main_filesystem ;
extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern LocationScheduler *location_scheduler;
extern CommsScheduler *comms_scheduler;
extern DTEHandler *dte_handler;
extern Scheduler *system_scheduler;
extern Logger *sensor_log;
extern Logger *system_log;
extern BLEService *ble_service;
extern OTAFileUpdater *ota_updater;
extern RGBLed *status_led;
extern Switch *saltwater_switch;
extern Switch *reed_switch;
extern BatteryMonitor *battery_monitor;

// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

using fsm_handle = GenTracker;

TEST_GROUP(Sm)
{
	FakeSwitch *fake_reed_switch;
	FakeSwitch *fake_saltwater_switch;
	FakeRGBLed *fake_status_led;
	LinuxTimer *linux_timer;
	MockBatteryMonitor *mock_battery_monitor;
	MockOTAFileUpdater *mock_ota_file_updater;
	MockBLEService * mock_ble_service;

	void setup() {
		main_filesystem = new MockFileSystem;
		linux_timer = new LinuxTimer;
		system_timer = linux_timer;
		configuration_store = new MockConfigurationStore;
		location_scheduler = new MockLocationScheduler;
		comms_scheduler = new MockCommsScheduler;
		mock_ota_file_updater = new MockOTAFileUpdater;
		ota_updater = mock_ota_file_updater;
		dte_handler = new DTEHandler;
		system_scheduler = new Scheduler(system_timer);
		sensor_log = new MockLog;
		system_log = new MockLog;
		mock_ble_service = new MockBLEService;
		ble_service = mock_ble_service;
		mock_battery_monitor = new MockBatteryMonitor;
		battery_monitor = mock_battery_monitor;
		fake_status_led = new FakeRGBLed("STATUS");
		status_led = fake_status_led;
		fake_saltwater_switch = new FakeSwitch(0, 0);
		saltwater_switch = fake_saltwater_switch;
		fake_reed_switch = new FakeSwitch(0, 0);
		reed_switch = fake_reed_switch;
	}

	void teardown() {
		delete main_filesystem;
		delete linux_timer;
		delete system_scheduler;
		delete location_scheduler;
		delete comms_scheduler;
		delete sensor_log;
		delete system_log;
		delete mock_ble_service;
		delete mock_ota_file_updater;
		delete mock_battery_monitor;
		delete fake_status_led;
		delete fake_reed_switch;
		delete fake_saltwater_switch;
	}
};

TEST(Sm, CheckBootFileSystemMountOk)
{
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("start").onObject(mock_battery_monitor);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	CHECK_EQUAL((int)RGBLedColor::WHITE, (int)fake_status_led->get_state());
}

TEST(Sm, CheckBootFileSystemFirstMountFail)
{
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("start").onObject(mock_battery_monitor);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
}

TEST(Sm, CheckBootFileSystemSecondMountFailAndEnterErrorState)
{
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	system_scheduler->run();
	CHECK_TRUE(fsm_handle::is_in_state<ErrorState>());
}

TEST(Sm, CheckBootFileSystemFormatFailAndEnterErrorState)
{
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(-1);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	system_scheduler->run();
	CHECK_TRUE(fsm_handle::is_in_state<ErrorState>());
}

TEST(Sm, CheckBootStateTransitionsToIdleState)
{
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("start").onObject(mock_battery_monitor);
	fsm_handle::start();
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(true);
	mock().expectOneCall("is_battery_level_low").onObject(configuration_store).andReturnValue(false);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());
	CHECK_TRUE(fake_reed_switch->is_started());
	CHECK_FALSE(fake_saltwater_switch->is_started());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchSwipeAndTransitionToOperationalConfigValid)
{
	mock().disable();
	fsm_handle::start();

	mock().enable();
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(true);
	mock().expectOneCall("is_battery_level_low").onObject(configuration_store).andReturnValue(false);

	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());
	CHECK_EQUAL((int)RGBLedColor::GREEN, (int)status_led->get_state());
	CHECK_FALSE(status_led->is_flashing());

	// After 120 seconds, transition to operational with green LED flashing
	mock().expectOneCall("start").onObject(location_scheduler).ignoreOtherParameters();
	mock().expectOneCall("start").onObject(comms_scheduler);
	mock().expectOneCall("is_battery_level_low").onObject(configuration_store).andReturnValue(false);
	linux_timer->set_counter(120999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OperationalState>());
	CHECK_EQUAL((int)RGBLedColor::GREEN, (int)status_led->get_state());
	CHECK_TRUE(status_led->is_flashing());

	// Green LED should go off after 5 seconds
	linux_timer->set_counter(125999);
	while(!system_scheduler->run());
	CHECK_EQUAL((int)RGBLedColor::BLACK, (int)status_led->get_state());
}


TEST(Sm, CheckWakeupToIdleWithReedSwitchSwipeAndTransitionToOperationalConfigValidBatteryLow)
{
	mock().disable();
	fsm_handle::start();

	mock().enable();
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(true);
	mock().expectOneCall("is_battery_level_low").onObject(configuration_store).andReturnValue(true);

	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());
	CHECK_EQUAL((int)RGBLedColor::YELLOW, (int)status_led->get_state());
	CHECK_FALSE(status_led->is_flashing());

	// After 120 seconds, transition to operational with yellow LED flashing
	mock().expectOneCall("start").onObject(gps_scheduler);
	mock().expectOneCall("start").onObject(comms_scheduler);
	mock().expectOneCall("is_battery_level_low").onObject(configuration_store).andReturnValue(true);
	linux_timer->set_counter(120999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OperationalState>());
	CHECK_EQUAL((int)RGBLedColor::YELLOW, (int)status_led->get_state());
	CHECK_TRUE(status_led->is_flashing());

	// Green LED should go off after 5 seconds
	linux_timer->set_counter(125999);
	while(!system_scheduler->run());
	CHECK_EQUAL((int)RGBLedColor::BLACK, (int)status_led->get_state());
}


TEST(Sm, CheckWakeupToIdleWithReedSwitchSwipeAndTransitionToErrorConfigInvalid)
{
	mock().disable();

	fsm_handle::start();

	mock().enable();
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(false);

	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());
	CHECK_FALSE(status_led->is_flashing());
	CHECK_EQUAL((int)RGBLedColor::RED, (int)status_led->get_state());

	// After 120 seconds, transition to error with red LED flashing
	linux_timer->set_counter(120999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<ErrorState>());
	CHECK_TRUE(status_led->is_flashing());
	CHECK_EQUAL((int)RGBLedColor::RED, (int)status_led->get_state());

	// Red LED should go off after 5 seconds and then transition to off state with white LED flashing
	mock().expectOneCall("stop").onObject(mock_battery_monitor).ignoreOtherParameters();
	linux_timer->set_counter(125999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
	CHECK_TRUE(status_led->is_flashing());
	CHECK_EQUAL((int)RGBLedColor::WHITE, (int)status_led->get_state());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchHoldAndTransitionToConfigurationState)
{
	mock().disable();

	fsm_handle::start();
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());

	// Swipe gesture and hold for 3 seconds
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(3999);
	while(!system_scheduler->run());
	fake_reed_switch->set_state(false);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());
	CHECK_TRUE(status_led->is_flashing());
	CHECK_EQUAL((int)RGBLedColor::BLUE, (int)status_led->get_state());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchHoldAndTransitionToOffState)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());

	// Swipe gesture and hold for 3 seconds
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(3999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());

	// Continue to hold for 7 more seconds
	linux_timer->set_counter(10999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
	fake_reed_switch->set_state(false);
	CHECK_EQUAL((int)RGBLedColor::WHITE, (int)status_led->get_state());
	CHECK_TRUE(status_led->is_flashing());
	CHECK_EQUAL(125, fake_status_led->m_period);

	// Continue to hold for 5 more seconds
	mock().enable();
	mock().expectOneCall("powerdown");
	linux_timer->set_counter(15999);
	while(!system_scheduler->run());
	CHECK_EQUAL((int)RGBLedColor::BLACK, (int)status_led->get_state());
}

TEST(Sm, CheckBLEInactivityTimeout)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());

	// Swipe gesture and hold for 3 seconds
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(3999);
	while(!system_scheduler->run());
	fake_reed_switch->set_state(false);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());

	// Wait until BLE inactivity timeout
	linux_timer->set_counter(363999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
}


TEST(Sm, CheckTransitionToConfigurationStateAndVerifyOTAUpdateEvents)
{
	// BootState -> IdleState
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());

	// Swipe gesture and hold for 3 seconds -> ConfigurationState
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(3999);
	while(!system_scheduler->run());
	fake_reed_switch->set_state(false);

	// Trigger BLE connected event
	BLEServiceEvent event;
	event.event_type = BLEServiceEventType::CONNECTED;
	mock_ble_service->invoke_event(event);
	CHECK_EQUAL((int)RGBLedColor::BLUE, (int)status_led->get_state());
	CHECK_FALSE(status_led->is_flashing());

	// Trigger OTA_START
	mock().enable();
	event.event_type = BLEServiceEventType::OTA_START;
	event.file_size = 0x12345678;
	event.crc32 = 0x87654321;
	event.file_id = (unsigned int)OTAFileIdentifier::MCU_FIRMWARE;
	mock().expectOneCall("start_file_transfer").onObject(mock_ota_file_updater).withUnsignedIntParameter("file_id", event.file_id).withUnsignedIntParameter("length", event.file_size).withUnsignedIntParameter("crc32", event.crc32);
	mock_ble_service->invoke_event(event);

	// Trigger OTA_FILE_DATA
	event.event_type = BLEServiceEventType::OTA_FILE_DATA;
	event.data = (void *)0x12345678;
	event.length = 0x87654321;
	mock().expectOneCall("write_file_data").onObject(mock_ota_file_updater).withParameter("data", event.data).withParameter("length", event.length);
	mock_ble_service->invoke_event(event);

	// Trigger OTA_ABORT
	event.event_type = BLEServiceEventType::OTA_ABORT;
	mock().expectOneCall("abort_file_transfer");
	mock_ble_service->invoke_event(event);

	// Trigger OTA_END
	event.event_type = BLEServiceEventType::OTA_END;
	mock().expectOneCall("complete_file_transfer").onObject(mock_ota_file_updater);
	mock_ble_service->invoke_event(event);
	mock().expectOneCall("apply_file_update").onObject(mock_ota_file_updater);
	while(!system_scheduler->run());
}
