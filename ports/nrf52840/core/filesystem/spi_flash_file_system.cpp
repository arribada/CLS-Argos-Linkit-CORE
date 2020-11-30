#include <algorithm>
#include <vector>
#include "spi_flash_file_system.hpp"
#include "IS25LP128F.hpp"
#include "bsp.h"
#include "nrfx_qspi.h"

void LFSSpiFlashFileSystem::init()
{
	nrf_qspi_cinstr_conf_t config;

	nrfx_qspi_init(&BSP::QSPI_Inits[BSP::QSPI_0].qspi_config, nullptr, nullptr);

	// Switch the Flash chip from SPI to QSPI
	config = NRFX_QSPI_DEFAULT_CINSTR(IS25LP128F::QPIEN, 0);
	// Keep IO3 high during as this is the reset line in SPI mode
    config.io3_level = true;
    nrfx_qspi_cinstr_xfer(&config, nullptr, nullptr);
}

int LFSSpiFlashFileSystem::read(lfs_block_t block, lfs_off_t off, void * buffer, lfs_size_t size)
{
	nrfx_err_t ret = nrfx_qspi_read(buffer, size, block + off);
	if (ret != NRFX_SUCCESS)
		return LFS_ERR_IO;
	
	return LFS_ERR_OK;
}

int LFSSpiFlashFileSystem::prog(lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
	nrfx_err_t ret = nrfx_qspi_write(buffer, size, block + off);
	if (ret != NRFX_SUCCESS)
		return LFS_ERR_IO;

	// Check the all bytes were written correctly
	std::vector<uint8_t> read_buffer;
	read_buffer.resize(size);

	read(block, 0, &read_buffer[0], read_buffer.size());

	if (memcmp( reinterpret_cast<const uint8_t *>(buffer), &read_buffer[0], size ))
		return LFS_ERR_CORRUPT;
	
	return LFS_ERR_OK;
}

int LFSSpiFlashFileSystem::erase(lfs_block_t block)
{
	nrfx_err_t ret = nrfx_qspi_erase(NRF_QSPI_ERASE_LEN_4KB, block);
	if (ret != NRFX_SUCCESS)
		return LFS_ERR_IO;
	
	// Check the block erased correctly
	std::vector<uint8_t> read_buffer;
	read_buffer.resize(get_block_size());

	read(block, 0, &read_buffer[0], read_buffer.size());

	// Check all bytes were correctly erased
	if (std::any_of(read_buffer.cbegin(), read_buffer.cend(), [](uint8_t i){ return i != 0xFF; }))
		return LFS_ERR_CORRUPT;
	
	return LFS_ERR_OK;
}

int LFSSpiFlashFileSystem::sync()
{
	return LFS_ERR_OK;
}




int LFSSpiFlashFileSystem::write_enable()
{
	nrfx_qspi_cinstr_xfer();
}