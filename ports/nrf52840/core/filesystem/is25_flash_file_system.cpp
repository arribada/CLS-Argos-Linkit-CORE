#include <algorithm>
#include <vector>
#include "is25_flash_file_system.hpp"
#include "IS25LP128F.hpp"
#include "bsp.hpp"
#include "debug.hpp"

void Is25FlashFileSystem::init()
{
	// Initialise the IS25LP128F flash chip and set it up for QSPI

	nrf_qspi_cinstr_conf_t config;
	uint8_t status;
	uint8_t rx_buffer[3];
	uint8_t tx_buffer[1];

    config.io2_level = false;
	// Keep IO3 high during transfers as this is the reset line in SPI mode
	// We'll make use of this line once the FLASH chip is in QSPI mode
    config.io3_level = true;
    config.wipwait   = false;
    config.wren      = false;

	// Read and check the SPI device ID matches the expected value
	config.opcode = IS25LP128F::RDJDID;
	config.length = NRF_QSPI_CINSTR_LEN_4B;

	nrfx_qspi_init(&BSP::QSPI_Inits[BSP::QSPI_0].config, nullptr, nullptr);

	nrfx_qspi_cinstr_xfer(&config, nullptr, rx_buffer);

    if (rx_buffer[0] != IS25LP128F::MANUFACTURER_ID ||
        rx_buffer[1] != IS25LP128F::MEMORY_TYPE_ID ||
        rx_buffer[2] != IS25LP128F::CAPACITY_ID)
    {
		DEBUG_ERROR("IS25LP128F not correctly identified");
        return;
    }

	// Switch to QSPI mode
	config.opcode = IS25LP128F::WRSR;
    tx_buffer[0] = IS25LP128F::STATUS_QE;
	config.length = NRF_QSPI_CINSTR_LEN_2B;
	config.wren = true;
	nrfx_qspi_cinstr_xfer(&config, tx_buffer, nullptr);

	// Wait for QSPI to be programmed
	config.opcode = IS25LP128F::RDSR;
	config.length = NRF_QSPI_CINSTR_LEN_2B;
	config.wren = false;
    do
    {
		nrfx_qspi_cinstr_xfer(&config, nullptr, rx_buffer);
		status = rx_buffer[0];		
    }
	while (status & IS25LP128F::STATUS_WIP);
}

// The maximum read size is 0x3FFFF, size must be a multiple of 4, buffer must be word aligned
int Is25FlashFileSystem::read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size)
{
	// Ensure any previous writes/erases have completed
	int sync_ret = sync();
	if (sync_ret)
		return sync_ret;

	//DEBUG_TRACE("QSPI Flash read(%lu %lu %lu)", block, off, size);
	nrfx_err_t ret = nrfx_qspi_read(buffer, size, block * get_block_size() + off);
	if (ret != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Error %d", ret);
		return LFS_ERR_IO;
	}

	return LFS_ERR_OK;
}

// The maximum program size is 0x3FFFF, size must be a multiple of 4, buffer must be word aligned
int Is25FlashFileSystem::prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	//DEBUG_TRACE("QSPI Flash prog(%lu %lu %lu)", block, off, size);

	// Ensure any previous writes/erases have completed
	int sync_ret = sync();
	if (sync_ret)
		return sync_ret;

	nrfx_err_t ret = nrfx_qspi_write(buffer, size, block * get_block_size() + off);
	if (ret != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Error %d", ret);
		return LFS_ERR_IO;
	}

	// Check that all bytes were written correctly
	std::vector<uint8_t> read_buffer;
	read_buffer.resize(size);

	read(block, off, &read_buffer[0], read_buffer.size());

	if (memcmp( reinterpret_cast<const uint8_t *>(buffer), &read_buffer[0], size ))
	{
		DEBUG_ERROR("QSPI Flash prog reported a bad write");
		return LFS_ERR_CORRUPT;
	}
	
	return LFS_ERR_OK;
}

int Is25FlashFileSystem::erase(lfs_block_t block)
{
	//DEBUG_TRACE("QSPI Flash erase(%lu)", block);

	// Ensure any previous writes/erases have completed
	int sync_ret = sync();
	if (sync_ret)
		return sync_ret;

	nrfx_err_t qspi_ret = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, block * get_block_size());
	if (qspi_ret != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Error %d", qspi_ret);
		return LFS_ERR_IO;
	}
	
	// Check the block erased correctly by reading it back
	std::vector<uint8_t> read_buffer;
	read_buffer.resize(get_block_size());

	int read_ret = read(block, 0, &read_buffer[0], read_buffer.size());
	if (read_ret)
		return read_ret;

	// Check all bytes were erased correctly
	if (std::any_of(read_buffer.cbegin(), read_buffer.cend(), [](uint8_t i){ return i != 0xFF; }))
	{
		DEBUG_ERROR("QSPI Flash erase failed to erase");
		return LFS_ERR_CORRUPT;
	}

	return LFS_ERR_OK;
}

int Is25FlashFileSystem::sync()
{
	//DEBUG_TRACE("QSPI Sync()");
	nrfx_err_t ret;
	do
	{
		ret = nrfx_qspi_mem_busy_check();
	}
	while (ret == NRFX_ERROR_BUSY);

	if (ret != NRFX_SUCCESS)
	{
		DEBUG_ERROR("QSPI IO Sync %d", ret);
		return LFS_ERR_IO;
	}

	return LFS_ERR_OK;
}