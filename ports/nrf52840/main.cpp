#include <iostream>

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log_redirect.h"
#include "nrfx_spim.h"
#include "ble_interface.hpp"
#include "dte_handler.hpp"
#include "is25_flash_file_system.hpp"
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
#include "ota_update_service.hpp"
#include "fake_gps_scheduler.hpp"
#include "fake_comms_scheduler.hpp"
#include "nrf_rtc.hpp"
#include "gpio.hpp"
#include "artic.hpp"
#include "nrf_led.hpp"
#include "fake_battery_mon.hpp"

FileSystem *main_filesystem;

ConfigurationStore *configuration_store;
BLEService *dte_service;
BLEService *ota_update_service;
CommsScheduler *comms_scheduler;
GPSScheduler *gps_scheduler;
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

    // Fake battery monitor
    FakeBatteryMonitor fake_battery_monitor;
    battery_monitor = &fake_battery_monitor;

	NrfSwitch nrf_reed_switch(BSP::GPIO::GPIO_SWS, 50);
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

	printf("GenTracker Booted\r\n");

	GPIOPins::clear(BSP::GPIO::GPIO_LED_RED);

	Is25FlashFileSystem lfs_file_system;
	main_filesystem = &lfs_file_system;

	lfs_file_system.init();

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

	dte_service = &BleInterface::get_instance();

	OTAUpdateService ota_service;
	ota_update_service = &ota_service;

	ArticTransceiver artic_transceiver;
	comms_scheduler = &artic_transceiver;

	FakeGPSScheduler fake_gps_scheduler;
	gps_scheduler = &fake_gps_scheduler;

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
