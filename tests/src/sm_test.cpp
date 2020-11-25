#include "gentracker.hpp"
#include "filesystem.hpp"
#include "timer.hpp"
#include "gps_scheduler.hpp"
#include "comms_scheduler.hpp"
#include "debug.hpp"

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"


#define BLOCK_COUNT   (256)
#define BLOCK_SIZE    (64*1024)
#define PAGE_SIZE     (256)

// Global contexts
FileSystem *main_filesystem = 0;
ConsoleLog *console_log = new ConsoleLog;
Timer *system_timer;
ConfigurationStore *configuration_store;
GPSScheduler *gps_scheduler;
CommsScheduler *comms_scheduler;
Scheduler *system_scheduler;
Logger *sensor_log;
Logger *system_log;
BLEService *dte_service;
BLEService *ota_update_service;


// Mocked classes
class MockTimer : public Timer {
public:
	uint64_t get_counter() {
		//return mock().actualCall("get_counter").returnLongIntValue();
		return 0;
	}
	void add_schedule(TimerSchedule &s) {}
	void cancel_schedule(TimerSchedule const &s) {}
	void cancel_schedule(TimerEvent const &e) {}
	void cancel_schedule(void *user_arg) {}
	void start() {
		mock().actualCall("start").onObject(this);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
};

class MockConfigurationStore : public ConfigurationStore {
public:
	void init() {
		mock().actualCall("init").onObject(this);
	}
	bool is_valid() {
		return mock().actualCall("is_valid").onObject(this).returnBoolValue();
	}
	void notify_saltwater_switch_state(bool state) {
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
};

class MockGPSScheduler : public GPSScheduler {
public:
	void start() {
		mock().actualCall("start").onObject(this);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) {
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
};

class MockArgosScheduler : public CommsScheduler {
public:
	void start() {
		mock().actualCall("start").onObject(this);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void notify_saltwater_switch_state(bool state) {
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
};

class MockLog : public Logger {
public:

	void create() {
		mock().actualCall("create").onObject(this);
	}

	void write(void *entry) {
		mock().actualCall("create").onObject(this).withParameter("entry", entry);
	}

	void read(void *entry, int index=0) {
		mock().actualCall("create").onObject(this).withParameter("entry", entry).withParameter("index", index);
	}

	unsigned int num_entries() {
		return mock().actualCall("num_entries").returnIntValue();
	}
};

class MockBLEService : public BLEService {
	void start(void (*on_connected)(void), void (*on_disconnected)(void)) {
		mock().actualCall("start").onObject(this).withParameter("on_connected", on_connected).withParameter("on_disconnected", on_disconnected);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
};

class MockFileSystem : public FileSystem {
public:
	int mount() {
		return mock().actualCall("mount").onObject(this).returnIntValue();
	}
	int umount() {
		return mock().actualCall("umount").onObject(this).returnIntValue();
	}
	int format() {
		return mock().actualCall("format").onObject(this).returnIntValue();
	}
};

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
		comms_scheduler = new MockArgosScheduler;
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
