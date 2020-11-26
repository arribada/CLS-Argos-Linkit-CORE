#include "gentracker.hpp"
#include "debug.hpp"

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


// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

using fsm_handle = GenTracker;

TEST_GROUP(Sm)
{
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
	}

	void teardown() {
		delete main_filesystem;
		delete system_timer;
		delete system_scheduler;
		delete gps_scheduler;
		delete comms_scheduler;
		delete sensor_log;
		delete system_log;
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
	ReedSwitchEvent e;
	e.state = true;
	fsm_handle::dispatch(e);
	CHECK_TRUE(fsm_handle::is_in_state<ConfigurationState>());
	mock().checkExpectations();
}
