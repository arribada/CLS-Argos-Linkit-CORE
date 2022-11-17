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
#include "debug.hpp"
#include <string>

static uint32_t m_reset_cause = 0;

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
	nrfx_wdt_init(&BSP::WDT_Inits[BSP::WDT].config, nullptr);
	// Channel ID is discarded as we don't care which channel ID is allocated
	nrfx_wdt_channel_id id;
	nrfx_wdt_channel_alloc(&id);
	nrfx_wdt_enable();
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
