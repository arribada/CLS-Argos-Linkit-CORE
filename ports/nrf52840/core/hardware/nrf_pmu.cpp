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
#include <string>

static uint32_t m_reset_cause = 0;

void PMU::initialise() {
#ifdef POWER_CONTROL_PIN
	GPIOPins::set(BSP::GPIO_POWER_CONTROL);
#endif

	m_reset_cause = NRF_POWER->RESETREAS;
	NRF_POWER->RESETREAS = 0xFFFFFFFF;
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

void PMU::reset(bool dfu_mode) {
	if (dfu_mode)
		sd_power_gpregret_set(0, 0x1);
	sd_nvic_SystemReset();
}

void PMU::powerdown() {
#ifdef POWER_CONTROL_PIN
	GPIOPins::clear(POWER_CONTROL_PIN);
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
		return "Soft Reset";
	else
		return "Power On Reset";
}

const std::string PMU::hardware_version()
{
#if 1 == HW_VERSION_DETECT
	// Try to read I2C register of MCP4716 (present on V2 but not V1)
	uint8_t xfer;
	nrfx_err_t error = nrfx_twim_rx(&BSP::I2C_Inits[MCP4716_DEVICE].twim, MCP4716_I2C_ADDR, &xfer, sizeof(xfer));
    if (error == NRFX_SUCCESS)
    	return "LinkIt V2";
    else
    	return "LinkIt V1";
#else
    return "Unknown";
#endif
}
