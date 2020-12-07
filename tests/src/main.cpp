#include "filesystem.hpp"
#include "console_log.hpp"
#include "timer.hpp"
#include "config_store.hpp"
#include "gps_scheduler.hpp"
#include "comms_scheduler.hpp"
#include "scheduler.hpp"
#include "logger.hpp"
#include "ble_service.hpp"
#include "led.hpp"
#include "switch.hpp"
#include "memory_access.hpp"

#include "CppUTest/CommandLineTestRunner.h"
#include "CppUTest/TestRegistry.h"
#include "CppUTestExt/MockSupportPlugin.h"

// Global contexts
FileSystem *main_filesystem ;
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
Switch *reed_switch;
Switch *saltwater_switch;
Led *red_led;
Led *green_led;
Led *blue_led;
MemoryAccess *memory_access;

MockSupportPlugin mockPlugin;

int main(int argc, char** argv)
{
    TestRegistry::getCurrentRegistry()->installPlugin(&mockPlugin);
    int exit_code = CommandLineTestRunner::RunAllTests(argc, argv);
    delete console_log;
    return exit_code;
}
