#include "is25_flash.h"

#include <stdbool.h>

#include "sdk_config.h"
#include "IS25LP128F.h"
#include "nrfx_qspi.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_delay.h"


void is25_flash_deinit(void)
{
	nrfx_qspi_uninit();
}

int is25_flash_init(void)
{
	// Initialise the IS25LP128F flash chip and set it up for QSPI

	nrfx_qspi_config_t qspi_config = {
            .xip_offset = 0, // Address offset in the external memory for Execute in Place operation
            {
                .sck_pin = NRF_GPIO_PIN_MAP(0, 19),
                .csn_pin = NRF_GPIO_PIN_MAP(0, 24),
                .io0_pin = NRF_GPIO_PIN_MAP(0, 21),
                .io1_pin = NRF_GPIO_PIN_MAP(0, 23),
                .io2_pin = NRF_GPIO_PIN_MAP(0, 22),
                .io3_pin = NRF_GPIO_PIN_MAP(1,  0),
            },
            {
                .readoc = NRF_QSPI_READOC_READ4IO, // Number of data lines and opcode used for reading
                .writeoc = NRF_QSPI_WRITEOC_PP4O,  // Number of data lines and opcode used for writing
                .addrmode = NRF_QSPI_ADDRMODE_24BIT,
                .dpmconfig = false, // Deep power-down mode enable
            },
            {
                .sck_delay = 0, // SCK delay in units of 62.5 ns  <0-255>
                .dpmen = false, // Deep power-down mode enable
                .spi_mode = NRF_QSPI_MODE_0,
                .sck_freq = NRF_QSPI_FREQ_32MDIV1, // See table above
            },
            .irq_priority = NRFX_QSPI_CONFIG_IRQ_PRIORITY
        };

	nrf_qspi_cinstr_conf_t config;
	uint8_t status;
	uint8_t rx_buffer[3];
	uint8_t tx_buffer[1];
	nrfx_err_t ret;

	ret = nrfx_qspi_init(&qspi_config, NULL, NULL);
	if (ret != NRFX_SUCCESS) {
		NRF_LOG_ERROR("QSPI init error - %04x", ret);
		return -1;
	}

    config.io2_level = false;
	// Keep IO3 high during transfers as this is the reset line in SPI mode
	// We'll make use of this line once the FLASH chip is in QSPI mode
    config.io3_level = true;
    config.wipwait   = false;

	// Issue a wake up command in case the device is in a deep sleep
	config.opcode = POWER_UP;
	config.length = NRF_QSPI_CINSTR_LEN_1B;
	config.wren = false;

	nrfx_qspi_cinstr_xfer(&config, NULL, NULL);

	nrf_delay_ms(1);

	// Read and check the SPI device ID matches the expected value
	config.opcode = RDJDID;
	config.length = NRF_QSPI_CINSTR_LEN_4B;
	config.wren = false;

	nrfx_qspi_cinstr_xfer(&config, NULL, rx_buffer);

    if (rx_buffer[0] != MANUFACTURER_ID ||
        rx_buffer[1] != MEMORY_TYPE_ID ||
        rx_buffer[2] != CAPACITY_ID)
    {
		//DEBUG_ERROR("IS25LP128F not correctly identified");
        return -1;
    }

	// Switch to QSPI mode
	config.opcode = WRSR;
    tx_buffer[0] = STATUS_QE;
	config.length = NRF_QSPI_CINSTR_LEN_2B;
	config.wren = true;
	nrfx_qspi_cinstr_xfer(&config, tx_buffer, NULL);

	// Wait for QSPI to be programmed
	config.opcode = RDSR;
	config.length = NRF_QSPI_CINSTR_LEN_2B;
	config.wren = false;
    do
    {
		nrfx_qspi_cinstr_xfer(&config, NULL, rx_buffer);
		status = rx_buffer[0];
    }
	while (status & STATUS_WIP);

    return 0;
}

// The maximum read size is 0x3FFFF, size must be a multiple of 4, buffer must be word aligned
int is25_flash_read(uint32_t addr, uint8_t * buffer, uint32_t size)
{
	nrfx_err_t ret = nrfx_qspi_read(buffer, size, addr);
	if (ret != NRFX_SUCCESS)
	{
		NRF_LOG_ERROR("QSPI IO Error %04x", ret);
		return -1;
	}

	return 0;
}

int is25_flash_erase(uint32_t block)
{
	nrfx_err_t ret;

	ret = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, block * IS25_BLOCK_SIZE);
	if (ret != NRFX_SUCCESS)
	{
		NRF_LOG_ERROR("QSPI IO Error %04x", ret);
		return -1;
	}

	// Wait for erase to complete
	// Expected max wait time is 300ms as this is how long a 4kB erase can take
	const uint32_t WAIT_TIME_US = 10;
	const uint32_t WAIT_ATTEMPTS = 30000;

	uint32_t remaining_attempts = WAIT_ATTEMPTS;                
	do
	{
		ret = nrfx_qspi_mem_busy_check();
		if (ret == NRFX_SUCCESS)
			break;

		nrf_delay_us(WAIT_TIME_US);
	} while (--remaining_attempts);

	if (ret != NRFX_SUCCESS)
	{
		NRF_LOG_ERROR("QSPI IO Error %04x", ret);
		return -1;
	}

	return 0;
}
