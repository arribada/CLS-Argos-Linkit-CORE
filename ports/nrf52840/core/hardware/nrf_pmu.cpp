#include "bsp.hpp"
#include "pmu.hpp"
#include "gpio.hpp"
#include "nrf_nvic.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_delay.h"
#include "nrfx_wdt.h"
#include "nrf_power.h"
#include "nrf_sdh.h"
#include "nrfx_twim.h"
#include "cm_backtrace.h"
#include "debug.hpp"
#include "crc16.h"
#include <string>

static uint32_t m_reset_cause = 0;

static __attribute__((section(".noinit"))) volatile uint32_t m_callstack[8];
static __attribute__((section(".noinit"))) volatile PMULogType m_type;
static __attribute__((section(".noinit"))) volatile uint16_t m_crc;

// Define a spare bit that we can use to detect pseudo power off
// via GPREGRET
#define POWER_RESETREAS_PSEUDO_POWER_OFF  0x80000000

void PMU::initialise() {
#ifdef POWER_CONTROL_PIN
	GPIOPins::set(BSP::GPIO_POWER_CONTROL);
#endif

	m_reset_cause = NRF_POWER->RESETREAS;
	NRF_POWER->RESETREAS = 0xFFFFFFFF; // Clear down

	// Apply pseudo power off flag is GPREGRET is set
	if (NRF_POWER->GPREGRET)
		m_reset_cause |= POWER_RESETREAS_PSEUDO_POWER_OFF;
	else
		m_reset_cause &= ~POWER_RESETREAS_PSEUDO_POWER_OFF;
	NRF_POWER->GPREGRET = 0; // Clear down
	nrf_pwr_mgmt_init();

#ifdef SOFTDEVICE_PRESENT
	if (nrf_sdh_is_enabled())
	{
		sd_power_dcdc0_mode_set(NRF_POWER_DCDC_ENABLE);
	}
	else
#endif
	{
		nrf_power_dcdcen_set(true);
	}
}

void PMU::reset(bool) {
	sd_nvic_SystemReset();
}

void PMU::powerdown() {
#if defined(POWER_CONTROL_PIN)
	DEBUG_TRACE("Attempt power off using power pin");
	GPIOPins::clear(POWER_CONTROL_PIN);
	PMU::delay_ms(1000); // If power on pin is connected then allow time for it to take effect
#endif

#if defined(PSEUDO_POWER_OFF)
	DEBUG_TRACE("Pseudo power off (soft reset)");
	// Mark this as a pseudo power off
	NRF_POWER->GPREGRET = 0x80;
	sd_nvic_SystemReset();
#endif

	// This is not a real powerdown but rather an infinite sleep
	for (;;) PMU::run();
}

void PMU::run() {
	nrf_pwr_mgmt_run();
}

void PMU::delay_ms(unsigned ms)
{
	nrf_delay_ms(ms);
}

void PMU::delay_us(unsigned us)
{
	nrf_delay_us(us);
}

void PMU::start_watchdog()
{
	nrfx_wdt_init(&BSP::WDT_Inits[BSP::WDT].config, PMU::watchdog_handler);
	// Channel ID is discarded as we don't care which channel ID is allocated
	nrfx_wdt_channel_id id;
	nrfx_wdt_channel_alloc(&id);
	nrfx_wdt_enable();
	cm_backtrace_init("", "", "");
}

void PMU::kick_watchdog()
{
	// Kicks all WDT channels
	nrfx_wdt_feed();
}

const std::string PMU::reset_cause()
{
	if (m_reset_cause & NRF_POWER_RESETREAS_RESETPIN_MASK)
		return "Hard Reset";
	else if (m_reset_cause & NRF_POWER_RESETREAS_DOG_MASK)
		return "WDT Reset";
	else if (m_reset_cause & NRF_POWER_RESETREAS_SREQ_MASK)
		if (m_reset_cause & POWER_RESETREAS_PSEUDO_POWER_OFF)
			return "Pseudo Power On Reset";
		else
			return "Soft Reset";
	else
		return "Power On Reset";
}

uint32_t PMU::device_identifier()
{
	return NRF_FICR->DEVICEID[0];
}

const std::string PMU::hardware_version()
{
#if 1 == HW_VERSION_DETECT
	// Try to read I2C register of MCP4716 (present on V2 but not V1)
	uint8_t xfer;
	nrfx_err_t error = nrfx_twim_rx(&BSP::I2C_Inits[MCP4716_DEVICE].twim, MCP4716_I2C_ADDR, &xfer, sizeof(xfer));
    if (error == NRFX_SUCCESS)
    	return "LinkIt V3";
    else
    	return "LinkIt V1";
#else
    return "Unknown";
#endif
}

void PMU::watchdog_handler() {
	save_stack(PMULogType::WDT);
}

void PMU::save_stack(PMULogType type) {
	m_type = type;
	uint32_t lr = (uint32_t)__builtin_return_address(0);
	uint32_t sp = (uint32_t)__builtin_frame_address(0);
	cm_backtrace_fault(lr, sp, (uint32_t *)m_callstack, sizeof(m_callstack) / sizeof(m_callstack[0]));
	m_crc = crc16_compute((const uint8_t *)m_callstack, sizeof(m_callstack), nullptr);
}

static const char *reset_type_to_string(PMULogType t) {
	switch (t) {
	case PMULogType::WDT:
		return "WDT";
	case PMULogType::HARDFAULT:
		return "HARDFAULT";
	case PMULogType::ETL:
		return "ETL";
	case PMULogType::MMAN:
		return "MMAN";
	case PMULogType::STACK:
		return "STACK";
	case PMULogType::MALLOC:
		return "MALLOC";
	default:
		return "UNKNOWN";
	}
}

void PMU::print_stack() {
	// Check CRC matches
	if (m_crc == crc16_compute((const uint8_t *)&m_callstack, sizeof(m_callstack), nullptr))
	{
		// Dump the information
		DEBUG_INFO("PMU post-reset trace available");
		DEBUG_INFO("PMU reset type: %s", reset_type_to_string(m_type));
		for (unsigned int i = 0; i < (sizeof(m_callstack) / sizeof(m_callstack[0])); i++)
			DEBUG_INFO("PMU PC[%u] = %08x", i, (unsigned int)m_callstack[i]);
	} else {
		DEBUG_TRACE("PMU post-reset trace unavailable (crc=%04x)", (unsigned int)m_crc);
	}

	m_crc = 0; // Invalidate CRC
	memset((void *)&m_callstack, sizeof(m_callstack), 0);
}
