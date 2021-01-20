#include <stdint.h>

#include "nrf_spim.hpp"
#include "bsp.hpp"
#include "nrf_gpio.h"
#include "nrfx_spim.h"
#include "nrf_pwr_mgmt.h"

static uint32_t sspin_internal[BSP::SPI::SPI_TOTAL_NUMBER] = {NRFX_SPIM_PIN_NOT_USED};

#ifdef SPI_USE_IRQ

static void event_handler(nrfx_spim_evt_t const * p_event, void * p_context)
{
	NrfSPIM *obj = static_cast<NrfSPIM *>(p_context);
    obj->m_xfer_done = true;
}

#endif


__RAMFUNC void activate_ss(uint32_t instance)
{
    if (sspin_internal[instance] != NRFX_SPIM_PIN_NOT_USED)
    {
        if (BSP::SPI_Inits[instance].spim_config.ss_active_high)
            nrf_gpio_pin_set(sspin_internal[instance]);
        else
            nrf_gpio_pin_clear(sspin_internal[instance]);
    }
}

__RAMFUNC void deactivate_ss(uint32_t instance)
{
    if (sspin_internal[instance] != NRFX_SPIM_PIN_NOT_USED)
    {
        if (BSP::SPI_Inits[instance].spim_config.ss_active_high)
            nrf_gpio_pin_clear(sspin_internal[instance]);
        else
            nrf_gpio_pin_set(sspin_internal[instance]);
    }
}


NrfSPIM::NrfSPIM(unsigned int instance)
{
	m_instance = instance;

	sspin_internal[instance] = BSP::SPI_Inits[instance].spim_config.ss_pin;

	if (BSP::SPI_Inits[instance].spim_config.ss_pin != NRFX_SPIM_PIN_NOT_USED)
	{
		deactivate_ss(instance);
	    nrf_gpio_cfg_output(sspin_internal[instance]);
	}

	BSP::SPI_InitTypeDefAndInst_t spi_init = BSP::SPI_Inits[instance];

	spi_init.spim_config.ss_pin = NRFX_SPIM_PIN_NOT_USED;

#ifdef SPI_USE_IRQ
	nrfx_spim_init(&spi_init.spim, &spi_init.spim_config, spim_event_handler, (void *)this);
#else
	nrfx_spim_init(&spi_init.spim, &spi_init.spim_config, NULL, NULL);
#endif
}


NrfSPIM::~NrfSPIM()
{
    nrfx_spim_uninit(&BSP::SPI_Inits[m_instance].spim);
}

int NrfSPIM::transfer(const uint8_t *tx_data, uint8_t *rx_data, uint16_t size)
{
	int ret;
    nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(tx_data, size, rx_data, size);

#ifdef SPI_USE_IRQ
    m_xfer_done = false;
#endif

    activate_ss(m_instance);

    ret = nrfx_spim_xfer(&BSP::SPI_Inits[m_instance].spim, &xfer_desc, 0);

#ifdef SPI_USE_IRQ
    while (!m_xfer_done)
        syshal_pmu_sleep(SLEEP_LIGHT);
#endif

    deactivate_ss(m_instance);

    return ret;
}

int NrfSPIM::transfer_continuous(const uint8_t *tx_data, uint8_t *rx_data, uint16_t size)
{
	int ret;
    nrfx_spim_xfer_desc_t xfer_desc = NRFX_SPIM_XFER_TRX(tx_data, size, rx_data, size);

#ifdef SPI_USE_IRQ
    m_xfer_done = false;
#endif

    activate_ss(m_instance);

    ret = nrfx_spim_xfer(&BSP::SPI_Inits[m_instance].spim, &xfer_desc, 0);

#ifdef SPI_USE_IRQ
    while (!m_xfer_done)
    	nrf_pwr_mgmt_run();
#endif

    return ret;
}

int NrfSPIM::finish_transfer()
{
	deactivate_ss(m_instance);

	return 0;
}
