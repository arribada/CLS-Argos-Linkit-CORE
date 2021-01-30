#include "gentracker.hpp"
#include "debug.hpp"

#include "fake_switch.hpp"
#include "fake_led.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_ble_serv.hpp"
#include "mock_comms.hpp"
#include "mock_config_store.hpp"
#include "mock_fs.hpp"
#include "mock_gps.hpp"
#include "mock_logger.hpp"
#include "mock_timer.hpp"
#include "scheduler.hpp"
#include "dte_handler.hpp"
#include "linux_timer.hpp"

#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)

// These are defined by main.cpp
extern FileSystem *main_filesystem ;
extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern GPSScheduler *gps_scheduler;
extern CommsScheduler *comms_scheduler;
extern DTEHandler *dte_handler;
extern Scheduler *system_scheduler;
extern Logger *sensor_log;
extern Logger *system_log;
extern BLEService *dte_service;
extern BLEService *ota_update_service;
extern Led *red_led;
extern Led *green_led;
extern Led *blue_led;
extern Switch *saltwater_switch;
extern Switch *reed_switch;


// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

using fsm_handle = GenTracker;

TEST_GROUP(Sm)
{
	FakeSwitch *fake_reed_switch;
	FakeSwitch *fake_saltwater_switch;
	FakeLed *fake_red_led;
	FakeLed *fake_green_led;
	FakeLed *fake_blue_led;
	LinuxTimer *linux_timer;

	void setup() {
		MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
		main_filesystem = new MockFileSystem;
		linux_timer = new LinuxTimer;
		system_timer = linux_timer;
		configuration_store = new MockConfigurationStore;
		gps_scheduler = new MockGPSScheduler;
		comms_scheduler = new MockCommsScheduler;
		dte_handler = new DTEHandler;
		system_scheduler = new Scheduler(system_timer);
		sensor_log = new MockLog;
		system_log = new MockLog;
		dte_service = new MockBLEService;
		ota_update_service = new MockBLEService;
		fake_red_led = new FakeLed("RED");
		red_led = fake_red_led;
		fake_green_led = new FakeLed("GREEN");
		green_led = fake_green_led;
		fake_blue_led = new FakeLed("BLUE");
		blue_led = fake_blue_led;
		fake_saltwater_switch = new FakeSwitch(0, 0);
		saltwater_switch = fake_saltwater_switch;
		fake_reed_switch = new FakeSwitch(0, 0);
		reed_switch = fake_reed_switch;
	}

	void teardown() {
		delete main_filesystem;
		delete linux_timer;
		delete system_scheduler;
		delete gps_scheduler;
		delete comms_scheduler;
		delete sensor_log;
		delete system_log;
		delete fake_red_led;
		delete fake_green_led;
		delete fake_blue_led;
		delete fake_reed_switch;
		delete fake_saltwater_switch;
		MemoryLeakWarningPlugin::restoreNewDeleteOverloads();
	}
};

TEST(Sm, CheckBootFileSystemMountOk)
{
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
}

TEST(Sm, CheckBootFileSystemFirstMountFail)
{
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
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

TEST(Sm, CheckBootStateTransitionsToOffState)
{
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	fsm_handle::start();
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
	CHECK_TRUE(fake_reed_switch->is_started());
	CHECK_FALSE(fake_saltwater_switch->is_started());
	CHECK_TRUE(fake_red_led->is_flashing());
	CHECK_TRUE(fake_green_led->is_flashing());
	CHECK_TRUE(fake_blue_led->is_flashing());
	linux_timer->set_counter(5999); // RED LED should go off at after 6 seconds
	while(!system_scheduler->run());
	CHECK_FALSE(fake_red_led->is_flashing());
	CHECK_FALSE(fake_red_led->get_state());
	CHECK_FALSE(fake_green_led->is_flashing());
	CHECK_FALSE(fake_green_led->get_state());
	CHECK_FALSE(fake_blue_led->is_flashing());
	CHECK_FALSE(fake_blue_led->get_state());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchSwipeAndTransitionToOperationalConfigValid)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	linux_timer->set_counter(5999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());

	mock().enable();
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(true);

	// Swipe gesture
	fake_reed_switch->set_state(true);
	fake_reed_switch->set_state(false);
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());
	CHECK_FALSE(fake_red_led->get_state());
	CHECK_TRUE(fake_green_led->get_state());
	CHECK_FALSE(fake_blue_led->get_state());

	// After 120 seconds, transition to operational with green LED flashing
	mock().expectOneCall("start").onObject(gps_scheduler);
	mock().expectOneCall("start").onObject(comms_scheduler);
	linux_timer->set_counter(125999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OperationalState>());
	CHECK_TRUE(fake_green_led->is_flashing());
	CHECK_FALSE(fake_red_led->is_flashing());
	CHECK_FALSE(fake_blue_led->is_flashing());

	// Green LED should go off
	linux_timer->set_counter(130999);
	while(!system_scheduler->run());
	CHECK_FALSE(fake_green_led->is_flashing());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchSwipeAndTransitionToErrorConfigInvalid)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	linux_timer->set_counter(5999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());

	mock().enable();
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(false);

	// Swipe gesture, red led should go solid
	fake_reed_switch->set_state(true);
	fake_reed_switch->set_state(false);
	CHECK_TRUE(fsm_handle::is_in_state<IdleState>());
	CHECK_TRUE(fake_red_led->get_state());
	CHECK_FALSE(fake_red_led->is_flashing());
	CHECK_FALSE(fake_blue_led->is_flashing());

	// After 120 seconds, transition to error with red LED flashing
	linux_timer->set_counter(125999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<ErrorState>());
	CHECK_TRUE(fake_red_led->is_flashing());

	// Red LED should go off after 5 seconds and then transition to off state with red LED flashing once more
	linux_timer->set_counter(130999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchHoldAndTransitionToConfigurationState)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	linux_timer->set_counter(5999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());

	// Swipe gesture and hold for 3 seconds
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(8999);
	mock().enable();
	mock().expectOneCall("start").onObject(dte_service).ignoreOtherParameters();
	mock().expectOneCall("start").onObject(ota_update_service).ignoreOtherParameters();
	while(!system_scheduler->run());
	fake_reed_switch->set_state(false);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());
	CHECK_TRUE(fake_blue_led->is_flashing());
}

TEST(Sm, CheckWakeupToIdleWithReedSwitchHoldAndTransitionToOffState)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	linux_timer->set_counter(5999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());

	// Swipe gesture and hold for 3 seconds
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(8999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());

	// Continue to hold for 7 more seconds
	linux_timer->set_counter(15999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
	fake_reed_switch->set_state(false);
}

TEST(Sm, CheckBLEInactivityTimeout)
{
	mock().disable();
	fsm_handle::start();
	while(!system_scheduler->run());
	linux_timer->set_counter(5999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());

	// Swipe gesture and hold for 3 seconds
	fake_reed_switch->set_state(true);
	linux_timer->set_counter(8999);
	while(!system_scheduler->run());
	fake_reed_switch->set_state(false);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());

	// Wait until BLE inactivity timeout
	linux_timer->set_counter(368999);
	while(!system_scheduler->run());
	CHECK_TRUE(fsm_handle::is_in_state<OffState>());
}
