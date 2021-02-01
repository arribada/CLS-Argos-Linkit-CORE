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
#include "fake_switch.hpp"
#include "fake_led.hpp"
#include "ota_update_service.hpp"
#include "fake_comms_scheduler.hpp"
#include "nrf_rtc.hpp"
#include "gpio.hpp"
#include "artic.hpp"
#include "m8q.hpp"


FileSystem *main_filesystem;

ConfigurationStore *configuration_store;
BLEService *dte_service;
BLEService *ota_update_service;
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

	// Set up fakes
	FakeSwitch fake_reed_switch(0, 0);
	reed_switch = &fake_reed_switch;
	FakeSwitch fake_saltwater_switch(0, 0);
	saltwater_switch = &fake_saltwater_switch;
	FakeLed fake_red_led("Red");
	red_led = &fake_red_led;
	FakeLed fake_green_led("Green");
	green_led = &fake_green_led;
	FakeLed fake_blue_led("Blue");
	blue_led = &fake_blue_led;

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
