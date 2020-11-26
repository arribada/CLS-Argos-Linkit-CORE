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



#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)

// These are defined by main.cpp
extern FileSystem *main_filesystem ;
extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern GPSScheduler *gps_scheduler;
extern CommsScheduler *comms_scheduler;
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

	void setup() {
		MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
		main_filesystem = new MockFileSystem;
		system_timer = new MockTimer;
		configuration_store = new MockConfigurationStore;
		gps_scheduler = new MockGPSScheduler;
		comms_scheduler = new MockCommsScheduler;
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
		delete system_timer;
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
	mock().expectOneCall("start").onObject(system_timer);
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	mock().checkExpectations();
}

TEST(Sm, CheckBootFileSystemFirstMountFail)
{
	mock().expectOneCall("start").onObject(system_timer);
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	mock().checkExpectations();
}

TEST(Sm, CheckBootFileSystemSecondMountFailAndEnterErrorState)
{
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	system_scheduler->run([](int e){}, 1);
	CHECK_TRUE(fsm_handle::is_in_state<ErrorState>());
	mock().checkExpectations();
}

TEST(Sm, CheckBootFileSystemFormatFailAndEnterErrorState)
{
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(-1);
	mock().expectOneCall("format").onObject(main_filesystem).andReturnValue(-1);
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	system_scheduler->run([](int e){}, 1);
	CHECK_TRUE(fsm_handle::is_in_state<ErrorState>());
	mock().checkExpectations();
}

TEST(Sm, CheckBootStateInvokesSchedulerToCheckConfigStore)
{
	mock().expectOneCall("start").onObject(system_timer);
	mock().expectOneCall("create").onObject(sensor_log);
	mock().expectOneCall("create").onObject(system_log);
	mock().expectOneCall("init").onObject(configuration_store);
	mock().expectOneCall("mount").onObject(main_filesystem).andReturnValue(0);
	mock().expectOneCall("is_valid").onObject(configuration_store).andReturnValue(false);
	fsm_handle::start();
	system_scheduler->run([](int e){}, 1);
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	mock().checkExpectations();
	CHECK_TRUE(fake_reed_switch->is_started());
	CHECK_TRUE(fake_saltwater_switch->is_started());
}

TEST(Sm, CheckBootTransitionsToOperationalWithValidConfig)
{
	mock().expectOneCall("start").onObject(gps_scheduler);
	mock().expectOneCall("start").onObject(comms_scheduler);
	mock().disable();
	fsm_handle::start();
	mock().enable();
	ConfigurationStatusEvent e;
	e.is_valid = true;
	fsm_handle::dispatch(e);
	CHECK_TRUE(fsm_handle::is_in_state<OperationalState>());
	mock().checkExpectations();
	CHECK_TRUE(fake_green_led->is_flashing());
}

TEST(Sm, CheckOperationalStateTransitionsToConfigState)
{
	mock().disable();
	fsm_handle::start();
	ConfigurationStatusEvent e;
	e.is_valid = true;
	fsm_handle::dispatch(e);
	CHECK_TRUE(fsm_handle::is_in_state<OperationalState>());
	fake_reed_switch->set_state(true);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());
	mock().enable();
	CHECK_FALSE(fake_green_led->is_flashing());
	CHECK_TRUE(fake_blue_led->is_flashing());
}

TEST(Sm, CheckRemainInBootStateWithoutValidConfig)
{
	mock().disable();
	fsm_handle::start();
	mock().enable();
	ConfigurationStatusEvent e;
	e.is_valid = false;
	fsm_handle::dispatch(e);
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
}

TEST(Sm, CheckBootEnterConfigStateOnReedSwitchActive)
{
	mock().expectOneCall("start").onObject(dte_service).ignoreOtherParameters();
	mock().expectOneCall("start").onObject(ota_update_service).ignoreOtherParameters();
	mock().disable();
	fsm_handle::start();
	mock().enable();

	// Inject the event directly
	ReedSwitchEvent e;
	e.state = true;
	fsm_handle::dispatch(e);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());
	mock().checkExpectations();
	CHECK_TRUE(fake_blue_led->is_flashing());

	// Inject the event from fake switch notification
	mock().disable();
	fsm_handle::start();
	CHECK_TRUE(fsm_handle::is_in_state<BootState>());
	fake_reed_switch->set_state(true);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());
	CHECK_TRUE(fake_blue_led->is_flashing());
}

TEST(Sm, CheckSaltwaterSwitchNotificationsDuringOperationalState)
{
	mock().disable();
	fsm_handle::start();

	{
		ConfigurationStatusEvent e;
		e.is_valid = true;
		fsm_handle::dispatch(e);
	}

	mock().enable();

	// Inject events directly

	mock().expectOneCall("notify_saltwater_switch_state").onObject(comms_scheduler).withParameter("state", true);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(gps_scheduler).withParameter("state", true);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(configuration_store).withParameter("state", true);

	{
		SaltwaterSwitchEvent e;
		e.state = true;
		fsm_handle::dispatch(e);
	}

	mock().expectOneCall("notify_saltwater_switch_state").onObject(comms_scheduler).withParameter("state", false);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(gps_scheduler).withParameter("state", false);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(configuration_store).withParameter("state", false);

	{
		SaltwaterSwitchEvent e;
		e.state = false;
		fsm_handle::dispatch(e);
	}

	// Inject events from fake switch
	mock().expectOneCall("notify_saltwater_switch_state").onObject(comms_scheduler).withParameter("state", true);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(gps_scheduler).withParameter("state", true);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(configuration_store).withParameter("state", true);
	fake_saltwater_switch->set_state(true);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(comms_scheduler).withParameter("state", false);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(gps_scheduler).withParameter("state", false);
	mock().expectOneCall("notify_saltwater_switch_state").onObject(configuration_store).withParameter("state", false);
	fake_saltwater_switch->set_state(false);

	mock().checkExpectations();
}
