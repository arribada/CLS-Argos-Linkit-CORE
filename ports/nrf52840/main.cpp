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
#include "fake_comms_scheduler.hpp"
#include "nrf_rtc.hpp"
#include "gpio.hpp"
#include "artic.hpp"
#include "is25_flash.hpp"
#include "nrf_led.hpp"
#include "nrf_battery_mon.hpp"
#include "m8q.hpp"

FileSystem *main_filesystem;

ConfigurationStore *configuration_store;
BLEService *ble_service;
OTAFileUpdater *ota_updater;
CommsScheduler *comms_scheduler;
LocationScheduler *location_scheduler;
MemoryAccess *memory_access;
Logger *sensor_log;
Logger *system_log;
ConsoleLog *console_log;
Timer *system_timer;
Scheduler *system_scheduler;
Led *red_led;
Led *green_led;
Led *blue_led;
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
	GPIOPins::set(BSP::GPIO_POWER_CONTROL);

	nrfx_uarte_init(&BSP::UART_Inits[BSP::UART_1].uarte, &BSP::UART_Inits[BSP::UART_1].config, nullptr);
    setvbuf(stdout, NULL, _IONBF, 0);

	ConsoleLog console_console_log;
	console_log = &console_console_log;

    nrf_log_redirect_init();

    NrfBatteryMonitor nrf_battery_monitor(BATTERY_ADC);
    battery_monitor = &nrf_battery_monitor;
	NrfSwitch nrf_reed_switch(BSP::GPIO::GPIO_REED_SW, REED_SWITCH_DEBOUNCE_TIME_MS);
	reed_switch = &nrf_reed_switch;
	SWS nrf_saltwater_switch;
	saltwater_switch = &nrf_saltwater_switch;
	NrfLed nrf_red_led("Red", BSP::GPIO::GPIO_LED_RED);
	red_led = &nrf_red_led;
	NrfLed nrf_green_led("Green", BSP::GPIO::GPIO_LED_GREEN);
	green_led = &nrf_green_led;
	NrfLed nrf_blue_led("Blue", BSP::GPIO::GPIO_LED_BLUE);
	blue_led = &nrf_blue_led;

    BleInterface::get_instance().init();

	system_timer = &NrfTimer::get_instance();
	NrfTimer::get_instance().init();

	rtc = &NrfRTC::get_instance();
	NrfRTC::get_instance().init();

	Scheduler scheduler(system_timer);
	system_scheduler = &scheduler;

	GPIOPins::clear(BSP::GPIO::GPIO_LED_RED);

	Is25Flash is25_flash;
	is25_flash.init();

	LFSFileSystem lfs_file_system(&is25_flash, IS25_BLOCK_COUNT - OTA_UPDATE_RESERVED_BLOCKS);
	main_filesystem = &lfs_file_system;

	LFSConfigurationStore store(lfs_file_system);
	configuration_store = &store;
	//store.init();

	NrfMemoryAccess nrf_memory_access;
	memory_access = &nrf_memory_access;

	ConsoleLog console_system_log;
	system_log = &console_system_log;

	ConsoleLog console_sensor_log;
	sensor_log = &console_sensor_log;

	DTEHandler dte_handler_local;
	dte_handler = &dte_handler_local;

	ble_service = &BleInterface::get_instance();
	OTAFlashFileUpdater ota_flash_file_updater(&lfs_file_system, &is25_flash, IS25_BLOCK_COUNT - OTA_UPDATE_RESERVED_BLOCKS, OTA_UPDATE_RESERVED_BLOCKS);
	ota_updater = &ota_flash_file_updater;

	ArticTransceiver artic_transceiver;
	comms_scheduler = &artic_transceiver;

	M8QReceiver m8q_gnss;
	location_scheduler = &m8q_gnss;

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
