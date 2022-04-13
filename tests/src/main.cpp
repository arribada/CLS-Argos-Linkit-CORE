#include "filesystem.hpp"
#include "console_log.hpp"
#include "timer.hpp"
#include "config_store.hpp"
#include "service_scheduler.hpp"
#include "dte_handler.hpp"
#include "scheduler.hpp"
#include "logger.hpp"
#include "ble_service.hpp"
#include "ota_file_updater.hpp"
#include "rgb_led.hpp"
#include "led.hpp"
#include "switch.hpp"
#include "memory_access.hpp"
#include "rtc.hpp"
#include "battery.hpp"
#include "debug.hpp"

#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Global contexts
FileSystem *main_filesystem ;
Timer *system_timer;
ConfigurationStore *configuration_store;
ServiceScheduler *comms_scheduler;
DTEHandler *dte_handler;
Scheduler *system_scheduler;
BLEService *ble_service;
OTAFileUpdater *ota_updater;
Switch *reed_switch;
RGBLed *status_led;
Led *ext_status_led;
MemoryAccess *memory_access;
RTC *rtc;
BatteryMonitor *battery_monitor;

MockSupportPlugin mockPlugin;

int main(int argc, char** argv)
{
	ConsoleLog con_log;
	DebugLogger::console_log = &con_log;
    TestRegistry::getCurrentRegistry()->installPlugin(&mockPlugin);
    int exit_code = CommandLineTestRunner::RunAllTests(argc, argv);
    return exit_code;
}
