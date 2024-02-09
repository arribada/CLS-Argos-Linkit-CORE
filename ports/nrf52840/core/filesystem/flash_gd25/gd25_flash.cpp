#include <algorithm>
#include <vector>
#include <iomanip>
#include "GD25Q16C.hpp"
#include "bsp.hpp"
#include "debug.hpp"
#include "gd25_flash.hpp"
#include "nrf_delay.h"

void Gd25Flash::init()
{
	// Initialise the GD25Q16C flash chip and set it up for QSPI

	nrf_qspi_cinstr_conf_t config;
	uint8_t status;
	uint8_t rx_buffer[3];
	uint8_t tx_buffer[2];

	// If setting the SO/io1 pin to an input is not done then the nrfx_qspi_init() timesout when the board
	// is reprogrammed using a JLink. It is unclear why this happens but having this in place causes no harm
	// This was found through experimentation and it is possible the root cause is merely masked by this
	nrf_gpio_cfg_input(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.io1_pin, NRF_GPIO_PIN_PULLDOWN);

	nrfx_err_t ret = nrfx_qspi_init(&BSP::QSPI_Inits[BSP::QSPI_0].config, nullptr, nullptr);
	if (ret != NRFX_SUCCESS)
	{
		DEBUG_ERROR("GD25Q16C QSPI initialisation failure - %d", ret);
        return;
	}

    config.io2_level = false;
	// Keep IO3 high during transfers as this is the reset line in SPI mode
	// We'll make use of this line once the FLASH chip is in QSPI mode
    config.io3_level = true;
    config.wipwait   = false;

	// Issue a wake up command in case the duevice is in a deep sleep
	config.opcode = GD25Q16C::RDPD;
	config.length = NRF_QSPI_CINSTR_LEN_1B;
	config.wren = false;

	nrfx_qspi_cinstr_xfer(&config, nullptr, nullptr);

	nrf_delay_ms(1);

	// Read and check the SPI device ID matches the expected value
	config.opcode = GD25Q16C::RDJDID;
	config.length = NRF_QSPI_CINSTR_LEN_4B;
	config.wren = false;

	nrfx_qspi_cinstr_xfer(&config, nullptr, rx_buffer);

    if (rx_buffer[0] != GD25Q16C::MANUFACTURER_ID ||
        rx_buffer[1] != GD25Q16C::MEMORY_TYPE_ID ||
        rx_buffer[2] != GD25Q16C::CAPACITY_ID)
    {
		nrfx_qspi_uninit();
		DEBUG_ERROR("GD25Q16C not correctly identified");
        return;
    }

	// Switch to QSPI mode
	config.opcode = GD25Q16C::WRSR;
	uint16_t tx_temp = GD25Q16C::STATUS_QE;
    tx_buffer[0] = (uint8_t) tx_temp;
    tx_buffer[1] = (uint8_t) (tx_temp >> 8);
	config.length = NRF_QSPI_CINSTR_LEN_3B;
	config.wren = true;
	nrfx_qspi_cinstr_xfer(&config, tx_buffer, nullptr);

	DEBUG_INFO("GD25Q16C set Qenabled and fast mode (%x , %x)", tx_buffer[0], tx_buffer[1]);
	// Wait for QSPI to be programmed
	config.opcode = GD25Q16C::RDSRL;
	config.length = NRF_QSPI_CINSTR_LEN_2B;
	config.wren = false;
    do
    {
		nrfx_qspi_cinstr_xfer(&config, nullptr, rx_buffer);
		status = rx_buffer[0];
    }
	while (status & (uint8_t) GD25Q16C::STATUS_WIP);
 	config.opcode = GD25Q16C::RDSRU;
	config.length = NRF_QSPI_CINSTR_LEN_2B;
	config.wren = false;
    //do
    {
		nrfx_qspi_cinstr_xfer(&config, nullptr, rx_buffer);
		status = rx_buffer[0];
    }
	//while (status & (uint8_t)(tx_temp >> 8)); /*/

	DEBUG_INFO("GD25Q16C Flash used and QUAD SPI configured (%x)", rx_buffer[0]);
	power_down();

	m_is_init = true;
}

// The maximum read size is 0x3FFFF, size must be a multiple of 4, buffer must be word aligned
int Gd25Flash::_read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size)
{
	// Check buffer alignment and size multiple
	if (((intptr_t)buffer & 3) || (size & 3))
		return LFS_ERR_INVAL;

	// Ensure no writes/erases are currently on going
	int ret_sync = _sync();
	if (ret_sync != LFS_ERR_OK)
		return ret_sync;

	//DEBUG_TRACE("QSPI Flash read(%lu %lu %lu)", block, off, size);
	nrfx_err_t ret = nrfx_qspi_read(buffer, size, block * m_block_size + off);
	if (ret != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Error %04x", ret);
		return LFS_ERR_IO;
	}

	return LFS_ERR_OK;
}

// The maximum program size is 0x3FFFF, size must be a multiple of 4, buffer must be word aligned
int Gd25Flash::_prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	//DEBUG_TRACE("QSPI Flash prog(%lu %lu %lu)", block, off, size);

	int ret_sync;

	// Check buffer alignment and size multiple
	if (((intptr_t)buffer & 3) || (size & 3))
		return LFS_ERR_INVAL;

	// Ensure no writes/erases are currently on going
	ret_sync = _sync();
	if (ret_sync != LFS_ERR_OK)
		return ret_sync;

	nrfx_err_t ret_write = nrfx_qspi_write(buffer, size, block * m_block_size + off);
	if (ret_write != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Error %04x", ret_write);
		return LFS_ERR_IO;
	}

	// Wait for the write to be completed before verifying it
	ret_sync = _sync();
	if (ret_sync != LFS_ERR_OK)
		return ret_sync;

	// Check that all bytes were written correctly
	uint8_t read_buffer[GD25_PAGE_SIZE];

	if (size > sizeof(read_buffer))
	{
		DEBUG_ERROR("QSPI Flash prog verification buffer too small, need size of %lu", size);
		return LFS_ERR_NOMEM;
	}

	int ret_read = _read(block, off, &read_buffer[0], size);
	if (ret_read != LFS_ERR_OK)
		return ret_read;

	if (memcmp( reinterpret_cast<const uint8_t *>(buffer), &read_buffer[0], size ))
	{
		DEBUG_ERROR("QSPI Flash prog reported a bad write");
#if (DEBUG_LEVEL >= 1)
		for (unsigned int i = 0; i < size; i++)
			printf("%02X", ((uint8_t *)buffer)[i]);
		printf("\r\n");
		for (unsigned int i = 0; i < size; i++)
			printf("%02X", read_buffer[i]);
		printf("\r\n");
#endif
		return LFS_ERR_CORRUPT;
	}
	
	return LFS_ERR_OK;
}

int Gd25Flash::_erase(lfs_block_t block)
{
	//DEBUG_TRACE("QSPI Flash erase(%lu)", block);

	// Ensure no writes/erases are currently on going
	int ret_sync = _sync();
	if (ret_sync != LFS_ERR_OK)
		return ret_sync;

	nrfx_err_t ret_erase = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, block * m_block_size);
	if (ret_erase != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Error %04x", ret_erase);
		return LFS_ERR_IO;
	}

	return LFS_ERR_OK;
}

int Gd25Flash::_sync()
{
	//DEBUG_TRACE("QSPI Sync()");
	nrfx_err_t ret;
	
	// Expected max wait time is 300ms as this is how long a 4kB erase can take
	constexpr uint32_t WAIT_TIME_US = 10;
	constexpr uint32_t WAIT_ATTEMPTS = 30000;

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
		DEBUG_ERROR("QSPI IO Sync %04x", ret);
		return LFS_ERR_IO;
	}

	return LFS_ERR_OK;
}

// Wrappers to ensure easier power up and power down of the QSPI peripheral
int Gd25Flash::read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size)
{
	if (!m_is_init)
		return LFS_ERR_IO;
	power_up();
	int ret = _read(block, off, buffer, size);
	power_down();
	return ret;
}

int Gd25Flash::prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	if (!m_is_init)
		return LFS_ERR_IO;
	power_up();
	int ret = _prog(block, off, buffer, size);
	power_down();
	return ret;
}

int Gd25Flash::erase(lfs_block_t block)
{
	if (!m_is_init)
		return LFS_ERR_IO;
	power_up();
	int ret = _erase(block);
	power_down();
	return ret;
}

int Gd25Flash::sync()
{
	if (!m_is_init)
		return LFS_ERR_IO;
	power_up();
	int ret = _sync();
	power_down();
	return ret;
}

void Gd25Flash::power_up()
{
	nrfx_qspi_init(&BSP::QSPI_Inits[BSP::QSPI_0].config, nullptr, nullptr);

	const nrf_qspi_cinstr_conf_t config =
	{
		.opcode = GD25Q16C::RDPD, // Wake from deep power mode
		.length = NRF_QSPI_CINSTR_LEN_1B,
		.io2_level = false,
		.io3_level = true,
		.wipwait = false,
		.wren = false
	};

	nrfx_qspi_cinstr_xfer(&config, nullptr, nullptr);

	nrf_delay_us(5);
}

void Gd25Flash::power_down()
{
	// Wait for any writes/erases to complete before sleeping
	_sync();

	const nrf_qspi_cinstr_conf_t config =
	{
		.opcode = GD25Q16C::DP, // Enter deep power mode
		.length = NRF_QSPI_CINSTR_LEN_1B,
		.io2_level = false,
		.io3_level = true,
		.wipwait = false,
		.wren = false
	};

	nrfx_qspi_cinstr_xfer(&config, nullptr, nullptr);

	nrf_delay_us(3);

	nrfx_qspi_uninit();

	nrf_gpio_cfg_output(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.csn_pin); // Keep CS high to ensure flash is not enabled
	nrf_gpio_pin_set(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.csn_pin);
	nrf_gpio_cfg_input(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.sck_pin, NRF_GPIO_PIN_PULLDOWN);
	nrf_gpio_cfg_input(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.io0_pin, NRF_GPIO_PIN_PULLDOWN);
	nrf_gpio_cfg_input(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.io1_pin, NRF_GPIO_PIN_PULLDOWN);
	nrf_gpio_cfg_input(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.io2_pin, NRF_GPIO_PIN_PULLDOWN);
	nrf_gpio_cfg_input(BSP::QSPI_Inits[BSP::QSPI_0].config.pins.io3_pin, NRF_GPIO_PIN_PULLDOWN);
}
