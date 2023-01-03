#include "sws_service.hpp"
#include "pressure_detector_service.hpp"
#include "als_sensor_service.hpp"
#include "ph_sensor_service.hpp"
#include "sea_temp_sensor_service.hpp"
#include "cdt_sensor_service.hpp"
#include "gps_service.hpp"
#include "pressure_sensor_service.hpp"
#include "axl_sensor_service.hpp"
#include "argos_tx_service.hpp"
#include "argos_rx_service.hpp"
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
#include "artic_sat.hpp"
#include "is25_flash.hpp"
#include "nrf_rgb_led.hpp"
#include "nrf_battery_mon.hpp"
#include "m8q.hpp"
#include "ltr_303.hpp"
#include "oem_ph.hpp"
#include "oem_rtd.hpp"
#include "cdt.hpp"
#include "bmx160.hpp"
#include "ms58xx.hpp"
#include "fs_log.hpp"
#include "nrf_i2c.hpp"
#include "gpio_led.hpp"
#include "heap.h"
#include "stwlc68.hpp"
#include "etl/error_handler.h"
#include "memory_monitor_service.hpp"
#include "dive_mode_service.hpp"


FileSystem *main_filesystem;

ConfigurationStore *configuration_store;
BLEService *ble_service;
OTAFileUpdater *ota_updater;
MemoryAccess *memory_access;
Timer *system_timer;
Scheduler *system_scheduler;
RGBLed *status_led;
Led *ext_status_led;
ReedSwitch *reed_switch;
DTEHandler *dte_handler;
RTC *rtc;
BatteryMonitor *battery_monitor;
BaseDebugMode g_debug_mode = BaseDebugMode::UART;
ArticDevice *artic_device;

static bool m_is_debug_init = false;

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
		PMU::kick_watchdog();
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
		PMU::kick_watchdog();
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
			PMU::kick_watchdog();
		}
#endif
	}
}

extern "C" void vApplicationMallocFailedHook() {
	for (;;)
	{
#if BUILD_TYPE==Release
		PMU::reset(false);
#else
		// Out of heap memory occurred
#ifdef GPIO_LED_REG
		GPIOPins::set(GPIO_LED_REG);
#endif
		GPIOPins::set(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::clear(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::clear(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(50);
		GPIOPins::set(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(50);
		PMU::kick_watchdog();
#endif
	}
}

void etl_error_handler(const etl::exception& e)
{
	DEBUG_TRACE("ETL error: %s in %s : %u", e.what(), e.file_name(), e.line_number());

	for (;;)
	{
#if BUILD_TYPE==Release
		PMU::reset(false);
#else
		// ETL error occurred
#ifdef GPIO_LED_REG
		GPIOPins::set(GPIO_LED_REG);
#endif
		GPIOPins::clear(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(200);
		GPIOPins::set(BSP::GPIO::GPIO_LED_RED);
		GPIOPins::set(BSP::GPIO::GPIO_LED_GREEN);
		GPIOPins::set(BSP::GPIO::GPIO_LED_BLUE);
		nrf_delay_ms(200);
		PMU::kick_watchdog();
#endif
	}
}

// Redirect std::cout and printf output to debug UART
// We have to define this as extern "C" as we are overriding a weak C function
extern "C" int _write(int file, char *ptr, int len)
{
	if (g_debug_mode == BaseDebugMode::UART && m_is_debug_init)
		nrfx_uarte_tx(&BSP::UART_Inits[BSP::UART_1].uarte, reinterpret_cast<const uint8_t *>(ptr), len);
	else if (ble_service && !__get_IPSR()) {
		ble_service->write(std::string(ptr, len));
	}
	return len;
}


int main()
{
	PMU::initialise();
	PMU::start_watchdog();
	PMU::kick_watchdog();
	GPIOPins::initialise();

	etl::error_handler::set_callback<etl_error_handler>();

#ifdef GPIO_AG_PWR_PIN
	// Current backfeeds from 3V3 -> i2c pullups -> BMX160 -> GPIO_AG_PWR
	// Because of this we need to float our GPIO_AG_PWR pin to avoid sinking that current and thus increasing our sleep current
	nrf_gpio_cfg_default(BSP::GPIO_Inits[GPIO_AG_PWR_PIN].pin_number);
#endif

	nrfx_uarte_init(&BSP::UART_Inits[BSP::UART_1].uarte, &BSP::UART_Inits[BSP::UART_1].config, nullptr);
	m_is_debug_init = true;
    setvbuf(stdout, NULL, _IONBF, 0);

	rtc = &NrfRTC::get_instance();
	NrfRTC::get_instance().init();

	ConsoleLog console_log;
	DebugLogger::console_log = &console_log;

    nrf_log_redirect_init();

    DEBUG_TRACE("Timer...");
	system_timer = &NrfTimer::get_instance();
	NrfTimer::get_instance().init();

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

	// Ext LED off
	if (ext_status_led)
		ext_status_led->off();

    DEBUG_TRACE("Reed switch...");
    NrfSwitch nrf_reed_switch(BSP::GPIO::GPIO_REED_SW, REED_SWITCH_DEBOUNCE_TIME_MS, REED_SWITCH_ACTIVE_STATE);

	DEBUG_TRACE("BLE...");
    BleInterface::get_instance().init();

	DEBUG_TRACE("IS25 flash...");
	Is25Flash is25_flash;
	is25_flash.init();

	// Check the reed switch is engaged for 3 seconds if this is a power on event
    DEBUG_TRACE("PMU Reset Cause = %s", PMU::reset_cause().c_str());

#ifdef POWER_ON_RESET_REQUIRES_REED_SWITCH
#ifdef PSEUDO_POWER_OFF

	NrfI2C::init();
	bool is_linkit_v3 = PMU::hardware_version() == "LinkIt V3";
	NrfI2C::uninit();

	if ((is_linkit_v3 && PMU::reset_cause() == "Pseudo Power On Reset") ||
		(!is_linkit_v3 && (PMU::reset_cause() == "Power On Reset" ||
				PMU::reset_cause() == "Pseudo Power On Reset"))) {

		if (PMU::reset_cause() == "Power On Reset")  {
			// Force GNSS off
			M8QReceiver m;
			m.power_off();

			// Force Artic PA off
			NrfI2C::init();
			ArticSat::shutdown();
			NrfI2C::uninit();
		}

		// De-initialize UART to save power
		m_is_debug_init = false;
		nrfx_uarte_uninit(&BSP::UART_Inits[BSP::UART_1].uarte);

		volatile bool power_on_ready = false;
		system_timer->start();
		Timer::TimerHandle timer_handle;

		// Check if switch starting state is active
		if (nrf_reed_switch.get_state()) {
			status_led->set(RGBLedColor::WHITE);
			timer_handle = system_timer->add_schedule([&power_on_ready]() {
				DEBUG_TRACE("Reed switch 3s period elapsed");
				power_on_ready = true;
			}, system_timer->get_counter() + 3000);
		} else {
		    // Turn status LED off
		    status_led->off();
		}

		nrf_reed_switch.start([&timer_handle, &power_on_ready](bool state) {
			system_timer->cancel_schedule(timer_handle);
			if (state) {
				DEBUG_TRACE("Reed State: %u", state);
				status_led->set(RGBLedColor::WHITE);
				timer_handle = system_timer->add_schedule([&power_on_ready]() {
					DEBUG_TRACE("Reed switch 3s period elapsed");
					power_on_ready = true;
				}, system_timer->get_counter() + 3000);
			} else {
				status_led->off();
			}
		});

		Timer::TimerHandle wdog_handle;

		std::function<void()> kick_watchdog = [&wdog_handle,&kick_watchdog]() {
			wdog_handle = system_timer->add_schedule([&wdog_handle,&kick_watchdog]() {
				PMU::kick_watchdog();
				kick_watchdog();
			}, system_timer->get_counter() + (14 * 60 * 1000));
		};

		// Kick the watchdog periodically to avoid a WDT reset
		kick_watchdog();

		while (!power_on_ready) {
			PMU::run();
		}

		InterruptLock lock;
		system_timer->cancel_schedule(wdog_handle);
		PMU::kick_watchdog();
		nrf_reed_switch.stop();

		// Forces timer to restart from zero
		system_timer->stop();
		system_timer->start();

		// Re-initialize UART
		nrfx_uarte_init(&BSP::UART_Inits[BSP::UART_1].uarte, &BSP::UART_Inits[BSP::UART_1].config, nullptr);
		m_is_debug_init = true;

	}
#else
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
#endif // POWER_CONTROL_PIN
#endif // POWER_ON_RESET_REQUIRES_REED_SWITCH

#if HAS_WCHG
	DEBUG_TRACE("STWLC68...");
    STWLC68::get_instance().init();
#else
	DEBUG_TRACE("STWLC68 not included");
#endif

	DEBUG_TRACE("Scheduler...");
	Scheduler scheduler(system_timer);
	system_scheduler = &scheduler;

	DEBUG_TRACE("Battery monitor...");

    NrfBatteryMonitor nrf_battery_monitor(BATTERY_ADC);
    battery_monitor = &nrf_battery_monitor;

    // Initialise the I2C drivers
    NrfI2C::init();

    DEBUG_TRACE("Reed gesture...");
	ReedSwitch reed_gesture_switch(nrf_reed_switch);
	reed_switch = &reed_gesture_switch;

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

	DEBUG_TRACE("PRESSURE Sensor Log...");
	PressureLogFormatter pressure_sensor_log_formatter;
	FsLog pressure_sensor_log(&lfs_file_system, "PRESSURE", 1024*1024);
	pressure_sensor_log.set_log_formatter(&pressure_sensor_log_formatter);

	DEBUG_TRACE("AXL Sensor Log...");
	AXLLogFormatter axl_sensor_log_formatter;
	FsLog axl_sensor_log(&lfs_file_system, "AXL", 1024*1024);
	axl_sensor_log.set_log_formatter(&axl_sensor_log_formatter);

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

	DEBUG_TRACE("SWS...");
	SWSService sws;

	DEBUG_TRACE("Artic R2...");
	try {
		static ArticSat artic;
		static ArgosTxService argos_tx_service(artic);
		static ArgosRxService argos_rx_service(artic);
		artic_device = &artic;
#ifdef ARTIC_I2C_BUS_CONFLICT
		// We need to mark the I2C bus as disabled since the pins now in use
		NrfI2C::disable(ARTIC_I2C_BUS_CONFLICT);
#endif
	} catch (...) {
		DEBUG_TRACE("Artic R2 not detected");
	}

	DEBUG_TRACE("GPS M8Q ...");
	try {
		static M8QReceiver m8q_gnss;
		static GPSService gps_service(m8q_gnss, &fs_sensor_log);
	} catch (...) {
		DEBUG_TRACE("GPS M8Q not detected");
	}

	DEBUG_TRACE("MS58xx...");
	MS58xxLL *ms58xx_devices[BSP::I2C_TOTAL_NUMBER];
	for (unsigned int i = 0; i < BSP::I2C_TOTAL_NUMBER; i++) {
		static unsigned int i2caddr[2] = { MS5803_ADDRESS, MS5837_ADDRESS };
		static std::string variant[2] = { MS5803_VARIANT, MS5837_VARIANT };
		for (unsigned int j = 0; j < 2; j++) {
			try {
				ms58xx_devices[i] = new MS58xxLL(i, i2caddr[j], variant[j]);
				DEBUG_TRACE("MS58xx: found on i2cbus=%u i2caddr=0x%02x", i, i2caddr[j]);
				break;
			} catch (...) {
				DEBUG_TRACE("MS58xx: not detected on i2cbus=%u i2caddr=0x%02x", i, i2caddr[j]);
				ms58xx_devices[i] = nullptr;
			}
		}
	}

	DEBUG_TRACE("AD5933...");
	AD5933LL *ad5933_devices[BSP::I2C_TOTAL_NUMBER];
	for (unsigned int i = 0; i < BSP::I2C_TOTAL_NUMBER; i++) {
		try {
			ad5933_devices[i] = new AD5933LL(i, AD5933_ADDRESS);
			DEBUG_TRACE("AD5933: found on i2cbus=%u i2caddr=0x%02x", i, AD5933_ADDRESS);
		} catch (...) {
			DEBUG_TRACE("AD5933: not detected on i2cbus=%u i2caddr=0x%02x", i, AD5933_ADDRESS);
			ad5933_devices[i] = nullptr;
		}
	}

	bool cdt_present = false;
	bool standalone_pressure = false;
	// Iterate twice to allows flags to be set
	for (unsigned int x = 0; x < 2; x++) {
		// Check available devices on each bus
		for (unsigned int i = 0; i < BSP::I2C_TOTAL_NUMBER; i++) {
			//DEBUG_TRACE("cdt=%u press=%u bus=%u ad5933=%p ms58xx=%p", cdt_present, standalone_pressure, i, ad5933_devices[i], ms58xx_devices[i]);
			if (!cdt_present && ad5933_devices[i] && ms58xx_devices[i]) {
				DEBUG_TRACE("CDT on bus %u...", i);
				cdt_present = true;
				static CDT cdt(*ms58xx_devices[i], *ad5933_devices[i]);
				static CDTSensorService cdt_sensor_service(cdt, &cdt_sensor_log);
			} else if (!standalone_pressure && ms58xx_devices[i]) {
				DEBUG_TRACE("Standalone Pressure Sensor on bus %u...", i);
				standalone_pressure = true;
				static MS58xx ms58xx_pressure_sensor(*ms58xx_devices[i]);
				static PressureDetectorService pressure_detector(ms58xx_pressure_sensor);
				static PressureSensorService pressure_sensor(ms58xx_pressure_sensor, &pressure_sensor_log);
			}
		}

		if (standalone_pressure && cdt_present)
			break;
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

	DEBUG_TRACE("BMX160...");
	try {
		static BMX160 bmx160;
		static AXLSensorService axl_sensor_service(bmx160, &axl_sensor_log);
	} catch (...) {
		DEBUG_TRACE("BMX160: not detected");
	}

	DEBUG_TRACE("Memory monitor...");
	MemoryMonitorService memory_monitor_service;

#ifdef WCHG_INTB_PIN
	DEBUG_TRACE("Dive mode monitor...");
	NrfIRQ wchg_irq(WCHG_INTB_PIN);
	DiveModeService dive_mode_service(nrf_reed_switch, wchg_irq);
#endif

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
