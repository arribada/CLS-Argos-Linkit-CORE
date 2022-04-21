#include "sws_service.hpp"
#include "pressure_detector_service.hpp"
#include "als_sensor_service.hpp"
#include "ph_sensor_service.hpp"
#include "sea_temp_sensor_service.hpp"
#include "cdt_sensor_service.hpp"
#include "gps_service.hpp"
#include "sys_log.hpp"
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
#include "reed.hpp"
#include "nrf_rtc.hpp"
#include "gpio.hpp"
#include "artic.hpp"
#include "is25_flash.hpp"
#include "nrf_rgb_led.hpp"
#include "nrf_battery_mon.hpp"
#include "m8q.hpp"
#include "ms5803.hpp"
#include "ltr_303.hpp"
#include "oem_ph.hpp"
#include "oem_rtd.hpp"
#include "cdt.hpp"
#include "fs_log.hpp"
#include "nrfx_twim.h"
#include "gpio_led.hpp"


FileSystem *main_filesystem;

ConfigurationStore *configuration_store;
BLEService *ble_service;
OTAFileUpdater *ota_updater;
ServiceScheduler *comms_scheduler;
MemoryAccess *memory_access;
Timer *system_timer;
Scheduler *system_scheduler;
RGBLed *status_led;
Led *ext_status_led;
ReedSwitch *reed_switch;
DTEHandler *dte_handler;
RTC *rtc;
BatteryMonitor *battery_monitor;

// FSM initial state -> BootState
FSM_INITIAL_STATE(GenTracker, BootState)

// Reserve the last 1MB of IS25 flash memory for firmware updates
#define OTA_UPDATE_RESERVED_BLOCKS ((1024 * 1024) / IS25_BLOCK_SIZE)

// Reed switch debouncing time (ms)
#define REED_SWITCH_DEBOUNCE_TIME_MS    25


extern "C" void HardFault_Handler() {
	for (;;)
	{
#if BUILD_TYPE==Release
		PMU::reset(false);
#else
		// Hardfault occurred
#ifdef GPIO_LED_REG
		GPIOPins::set(GPIO_LED_REG);
#endif
		GPIOPins::clear(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(50);
		GPIOPins::set(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(50);
#endif
	}
}

extern "C" void MemoryManagement_Handler(void)
{
	for (;;)
	{
#if BUILD_TYPE==Release
		PMU::reset(false);
#else
		// Stack overflow detected
#ifdef GPIO_LED_REG
		GPIOPins::set(GPIO_LED_REG);
#endif
		GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::set(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(50);
		GPIOPins::clear(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::clear(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(50);
#endif
	}
}

extern "C" {
	void *__stack_check_guard = (void*)0xDEADBEEF;
	void __wrap___stack_chk_fail(void) {
#if BUILD_TYPE==Release
		PMU::reset(false);
#else
		for (;;)
		{
			// Stack corruption detected
#ifdef GPIO_LED_REG
			GPIOPins::set(GPIO_LED_REG);
#endif
			GPIOPins::set(BSP::GPIO::GPIO_LED_RED);
			GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
			GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
			nrf_delay_ms(50);
			GPIOPins::clear(BSP::GPIO::GPIO_LED_RED);
			GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
			GPIOPins::clear(BSP::GPIO::GPIO_LED_BLUE);
			nrf_delay_ms(50);
		}
#endif
	}
}

// Redirect std::cout and printf output to debug UART
// We have to define this as extern "C" as we are overriding a weak C function
extern "C" int _write(int file, char *ptr, int len)
{
	nrfx_uarte_tx(&BSP::UART_Inits[BSP::UART_1].uarte, reinterpret_cast<const uint8_t *>(ptr), len);
	return len;
}

int main()
{
	PMU::initialise();
	PMU::start_watchdog();
	GPIOPins::initialise();

#ifdef GPIO_AG_PWR_PIN
	// Current backfeeds from 3V3 -> i2c pullups -> BMX160 -> GPIO_AG_PWR
	// Because of this we need to float our GPIO_AG_PWR pin to avoid sinking that current and thus increasing our sleep current
	nrf_gpio_cfg_default(BSP::GPIO_Inits[GPIO_AG_PWR_PIN].pin_number);
#endif

	nrfx_uarte_init(&BSP::UART_Inits[BSP::UART_1].uarte, &BSP::UART_Inits[BSP::UART_1].config, nullptr);
    setvbuf(stdout, NULL, _IONBF, 0);

	rtc = &NrfRTC::get_instance();
	NrfRTC::get_instance().init();

	ConsoleLog console_log;
	DebugLogger::console_log = &console_log;

    nrf_log_redirect_init();

	// Check the reed switch is engaged for 3 seconds if this is a power on event
    DEBUG_TRACE("PMU Reset Cause = %s", PMU::reset_cause().c_str());

#ifdef POWER_ON_RESET_REQUIRES_REED_SWITCH
	if (PMU::reset_cause() == "Power On Reset") {
		unsigned int countdown = 3000;
		DEBUG_TRACE("Enter Power On Reed Switch Check");
		while (countdown) {
			//DEBUG_TRACE("Reed Switch: %u", GPIOPins::value(BSP::GPIO_REED_SW));
			if (GPIOPins::value(BSP::GPIO_REED_SW) != REED_SWITCH_ACTIVE_STATE)
				break;
			PMU::delay_ms(1);
			countdown--;
		}

		if (countdown) {
			DEBUG_TRACE("Reed Switch Inactive -- Powering Down...");
			PMU::powerdown();
		}
		DEBUG_TRACE("Exiting Power On Reed Switch Check");
	}
#endif

    // Initialise the I2C driver
	for (uint32_t i = 0; i < BSP::I2C_TOTAL_NUMBER; i++)
	{
		nrfx_twim_init(&BSP::I2C_Inits[i].twim, &BSP::I2C_Inits[i].twim_config, nullptr, nullptr);
		nrfx_twim_enable(&BSP::I2C_Inits[i].twim);
	}

    DEBUG_TRACE("Timer...");
	system_timer = &NrfTimer::get_instance();
	NrfTimer::get_instance().init();

	DEBUG_TRACE("Scheduler...");
	Scheduler scheduler(system_timer);
	system_scheduler = &scheduler;

	DEBUG_TRACE("RGB LED...");
	NrfRGBLed nrf_status_led("STATUS", BSP::GPIO::GPIO_LED_RED, BSP::GPIO::GPIO_LED_GREEN, BSP::GPIO::GPIO_LED_BLUE, RGBLedColor::WHITE);
	status_led = &nrf_status_led;

#ifdef EXT_LED_PIN
	DEBUG_TRACE("External LED...");
	GPIOLed gpio_led(EXT_LED_PIN);
	ext_status_led = &gpio_led;
#else
	ext_status_led = nullptr;
#endif

	DEBUG_TRACE("Battery monitor...");

    NrfBatteryMonitor nrf_battery_monitor(BATTERY_ADC);
    battery_monitor = &nrf_battery_monitor;

    DEBUG_TRACE("Reed switch...");
    NrfSwitch nrf_reed_switch(BSP::GPIO::GPIO_REED_SW, REED_SWITCH_DEBOUNCE_TIME_MS, REED_SWITCH_ACTIVE_STATE);

    DEBUG_TRACE("Reed gesture...");
	ReedSwitch reed_gesture_switch(nrf_reed_switch);
	reed_switch = &reed_gesture_switch;

	DEBUG_TRACE("SWS...");
	SWSService sws();

	DEBUG_TRACE("MS5803...");
	try {
		static MS5803 ms5803_pressure_sensor;
		static PressureDetectorService pressure_detector(ms5803_pressure_sensor);
	} catch (...) {
		DEBUG_TRACE("MS5803: not detected");
	}

	DEBUG_TRACE("BLE...");
    BleInterface::get_instance().init();

	DEBUG_TRACE("IS25 flash...");
	Is25Flash is25_flash;
	is25_flash.init();

	DEBUG_TRACE("LFS filesystem...");
	LFSFileSystem lfs_file_system(&is25_flash, IS25_BLOCK_COUNT - OTA_UPDATE_RESERVED_BLOCKS);
	main_filesystem = &lfs_file_system;

	// If we can't mount the filesystem then try to format it first and retry
	DEBUG_TRACE("Mount LFS filesystem...");
	if (main_filesystem->mount() < 0)
	{
		DEBUG_TRACE("Format LFS filesystem...");
		if (main_filesystem->format() < 0 || main_filesystem->mount() < 0)
		{
			// We can't mount a formatted filesystem, something bad has happened
			DEBUG_ERROR("Failed to format LFS filesystem");
			PMU::powerdown();
		}
	}

	DEBUG_TRACE("LFS System Log...");
	SysLogFormatter sys_log_formatter;
	FsLog fs_system_log(&lfs_file_system, "system.log", 1024*1024);
	fs_system_log.set_log_formatter(&sys_log_formatter);
	DebugLogger::system_log = &fs_system_log;

	DEBUG_TRACE("LFS (GPS) Sensor Log...");
	GPSLogFormatter fs_sensor_log_formatter;
	FsLog fs_sensor_log(&lfs_file_system, "sensor.log", 1024*1024);
	fs_sensor_log.set_log_formatter(&fs_sensor_log_formatter);

	DEBUG_TRACE("ALS Sensor Log...");
	ALSLogFormatter als_sensor_log_formatter;
	FsLog als_sensor_log(&lfs_file_system, "ALS", 1024*1024);
	als_sensor_log.set_log_formatter(&als_sensor_log_formatter);

	DEBUG_TRACE("PH Sensor Log...");
	PHLogFormatter ph_sensor_log_formatter;
	FsLog ph_sensor_log(&lfs_file_system, "PH", 1024*1024);
	ph_sensor_log.set_log_formatter(&ph_sensor_log_formatter);

	DEBUG_TRACE("RTD Sensor Log...");
	SeaTempLogFormatter rtd_sensor_log_formatter;
	FsLog rtd_sensor_log(&lfs_file_system, "RTD", 1024*1024);
	rtd_sensor_log.set_log_formatter(&rtd_sensor_log_formatter);

	DEBUG_TRACE("CDT Sensor Log...");
	CDTLogFormatter cdt_sensor_log_formatter;
	FsLog cdt_sensor_log(&lfs_file_system, "CDT", 1024*1024);
	cdt_sensor_log.set_log_formatter(&cdt_sensor_log_formatter);

	DEBUG_TRACE("Configuration store...");
	LFSConfigurationStore store(lfs_file_system);
	configuration_store = &store;

	DEBUG_TRACE("RAM access...");
	NrfMemoryAccess nrf_memory_access;
	memory_access = &nrf_memory_access;

	DEBUG_TRACE("DTE handler...");
	DTEHandler dte_handler_local;
	dte_handler = &dte_handler_local;

	DEBUG_TRACE("OTA updater...");
	ble_service = &BleInterface::get_instance();
	OTAFlashFileUpdater ota_flash_file_updater(&lfs_file_system, &is25_flash, IS25_BLOCK_COUNT - OTA_UPDATE_RESERVED_BLOCKS, OTA_UPDATE_RESERVED_BLOCKS);
	ota_updater = &ota_flash_file_updater;

	DEBUG_TRACE("Artic R2...");
	ArticTransceiver artic_transceiver;
	comms_scheduler = &artic_transceiver;

	DEBUG_TRACE("GPS M8Q ...");
	try {
		static M8QReceiver m8q_gnss;
		m8q_gnss.power_off(); // Forcibly power off
		static GPSService gps_service(m8q_gnss, &fs_sensor_log);
	} catch (...) {
		DEBUG_TRACE("GPS M8Q not detected");
	}

	DEBUG_TRACE("LTR303...");
	try {
		static LTR303 ltr303;
		static ALSSensorService als_sensor_service(ltr303, &als_sensor_log);
	} catch (...) {
		DEBUG_TRACE("LTR303: not detected");
	}

	DEBUG_TRACE("OEM PH...");
	try {
		static OEM_PH_Sensor ph;
		static PHSensorService ph_sensor_service(ph, &ph_sensor_log);
	} catch (...) {
		DEBUG_TRACE("OEM PH: not detected");
	}

	DEBUG_TRACE("OEM RTD...");
	try {
		static OEM_RTD_Sensor rtd;
		static SeaTempSensorService rtd_sensor_service(rtd, &rtd_sensor_log);
	} catch (...) {
		DEBUG_TRACE("OEM RTD: not detected");
	}

	DEBUG_TRACE("CDT...");
	try {
		static CDT cdt;
		static CDTSensorService cdt_sensor_service(cdt, &cdt_sensor_log);
	} catch (...) {
		DEBUG_TRACE("CDT: not detected");
	}

	DEBUG_TRACE("Entering main SM...");

	// This will initialise the FSM
	GenTracker::start();

	// The scheduler should run forever.  Any run-time exceptions should be handled and passed to FSM.
	while (true)
	{
		try {
			system_scheduler->run();
			PMU::run();
		} catch (ErrorCode e) {
			ErrorEvent event;
			event.error_code = e;
			GenTracker::dispatch(event);
		}
	}

	return 0;
}
