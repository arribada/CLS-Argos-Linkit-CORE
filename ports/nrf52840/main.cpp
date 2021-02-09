#include <iostream>

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log_redirect.h"
#include "nrfx_spim.h"
#include "ble_interface.hpp"
#include "ota_flash_file_updater.hpp"
#include "dte_handler.hpp"
#include "nrf_memory_access.hpp"
#include "config_store_fs.hpp"
#include "debug.hpp"
#include "console_log.hpp"
#include "debug.hpp"
#include "bsp.hpp"
#include "gentracker.hpp"
#include "nrf_timer.hpp"
#include "nrf_switch.hpp"
#include "sws.hpp"
#include "fake_gps_scheduler.hpp"
#include "fake_comms_scheduler.hpp"
#include "nrf_rtc.hpp"
#include "gpio.hpp"
#include "artic.hpp"
#include "is25_flash.hpp"
#include "nrf_rgb_led.hpp"
#include "nrf_battery_mon.hpp"
#include "fs_log.hpp"

FileSystem *main_filesystem;

ConfigurationStore *configuration_store;
BLEService *ble_service;
OTAFileUpdater *ota_updater;
CommsScheduler *comms_scheduler;
GPSScheduler *gps_scheduler;
MemoryAccess *memory_access;
Logger *sensor_log;
Logger *system_log;
ConsoleLog *console_log;
Timer *system_timer;
Scheduler *system_scheduler;
RGBLed *status_led;
Switch *saltwater_switch;
Switch *reed_switch;
DTEHandler *dte_handler;
RTC *rtc;
BatteryMonitor *battery_monitor;

// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

// Reserve the last 1MB of IS25 flash memory for firmware updates
#define OTA_UPDATE_RESERVED_BLOCKS ((1024 * 1024) / IS25_BLOCK_SIZE)

// Reed switch debouncing time (ms)
#define REED_SWITCH_DEBOUNCE_TIME_MS    50

// Redirect std::cout and printf output to debug UART
// We have to define this as extern "C" as we are overriding a weak C function
extern "C" int _write(int file, char *ptr, int len)
{
	nrfx_uarte_tx(&BSP::UART_Inits[BSP::UART_1].uarte, reinterpret_cast<const uint8_t *>(ptr), len);
	return len;
}

int main()
{
	GPIOPins::initialise();

	nrfx_uarte_init(&BSP::UART_Inits[BSP::UART_1].uarte, &BSP::UART_Inits[BSP::UART_1].config, nullptr);
    setvbuf(stdout, NULL, _IONBF, 0);

	ConsoleLog console_console_log;
	console_log = &console_console_log;

    nrf_log_redirect_init();

    DEBUG_INFO("Running main setup code...");

    DEBUG_INFO("Battery monitor...");

    NrfBatteryMonitor nrf_battery_monitor(BATTERY_ADC);
    battery_monitor = &nrf_battery_monitor;

    DEBUG_INFO("Reed switch...");
    NrfSwitch nrf_reed_switch(BSP::GPIO::GPIO_REED_SW, REED_SWITCH_DEBOUNCE_TIME_MS);
	reed_switch = &nrf_reed_switch;

	DEBUG_INFO("SWS...");
	SWS nrf_saltwater_switch;
	saltwater_switch = &nrf_saltwater_switch;

	DEBUG_INFO("LED...");
	NrfRGBLed nrf_status_led("STATUS", BSP::GPIO::GPIO_LED_RED, BSP::GPIO::GPIO_LED_GREEN, BSP::GPIO::GPIO_LED_BLUE, RGBLedColor::WHITE);
	status_led = &nrf_status_led;

    DEBUG_INFO("BLE...");
    BleInterface::get_instance().init();

    DEBUG_INFO("Timer...");
	system_timer = &NrfTimer::get_instance();
	NrfTimer::get_instance().init();

    DEBUG_INFO("RTC...");
	rtc = &NrfRTC::get_instance();
	NrfRTC::get_instance().init();

    DEBUG_INFO("Scheduler...");
	Scheduler scheduler(system_timer);
	system_scheduler = &scheduler;

    DEBUG_INFO("IS25 flash...");
	Is25Flash is25_flash;
	is25_flash.init();

    DEBUG_INFO("LFS filesystem...");
	LFSFileSystem lfs_file_system(&is25_flash, IS25_BLOCK_COUNT - OTA_UPDATE_RESERVED_BLOCKS);
	main_filesystem = &lfs_file_system;

    DEBUG_INFO("LFS System Log...");
	FsLog fs_system_log(&lfs_file_system, "system.log", 1024*1024);
	system_log = &fs_system_log;

    DEBUG_INFO("LFS Sensor Log...");
	FsLog fs_sensor_log(&lfs_file_system, "sensor.log", 1024*1024);
	sensor_log = &fs_sensor_log;

    DEBUG_INFO("Configuration store...");
	LFSConfigurationStore store(lfs_file_system);
	configuration_store = &store;

    DEBUG_INFO("RAM access...");
	NrfMemoryAccess nrf_memory_access;
	memory_access = &nrf_memory_access;

    DEBUG_INFO("DTE handler...");
	DTEHandler dte_handler_local;
	dte_handler = &dte_handler_local;

    DEBUG_INFO("OTA updater...");
	ble_service = &BleInterface::get_instance();
	OTAFlashFileUpdater ota_flash_file_updater(&lfs_file_system, &is25_flash, IS25_BLOCK_COUNT - OTA_UPDATE_RESERVED_BLOCKS, OTA_UPDATE_RESERVED_BLOCKS);
	ota_updater = &ota_flash_file_updater;

    DEBUG_INFO("Artic R2...");
	ArticTransceiver artic_transceiver;
	comms_scheduler = &artic_transceiver;

    DEBUG_INFO("GPS Scheduler...");
	FakeGPSScheduler fake_gps_scheduler;
	gps_scheduler = &fake_gps_scheduler;

    DEBUG_INFO("Entering main SM...");
	// This will initialise the FSM
	GenTracker::start();

	// The scheduler should run forever.  Any run-time exceptions should be handled and passed to FSM.
	try {
		while (true) {
			system_scheduler->run();
		}
	} catch (ErrorCode e) {
		ErrorEvent event;
		event.error_code = e;
		GenTracker::dispatch(event);
	}

	return 0;
}
