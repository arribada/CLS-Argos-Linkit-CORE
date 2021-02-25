#include "dfu.hpp"

#include "nrf_dfu_settings.h"
#include "nrf_dfu_types.h"
#include "debug.hpp"

void DFU::initialise()
{
	nrf_dfu_settings_init(true);
}

void DFU::write_ext_flash_dfu_settings(uint32_t src_addr, uint32_t image_size, uint32_t crc)
{
	DEBUG_TRACE("DFU::write_ext_flash_dfu_settings: updating");
	s_dfu_settings.bank_1.image_size = image_size;
	s_dfu_settings.bank_1.image_crc = crc;
	s_dfu_settings.bank_1.bank_code = NRF_DFU_BANK_VALID_EXT_APP;
	s_dfu_settings.progress.update_start_address = src_addr;
	s_dfu_settings.write_offset = 0;
	nrf_dfu_settings_write_and_backup(nullptr);
}

bool DFU::update_pending()
{
	return s_dfu_settings.bank_1.bank_code == NRF_DFU_BANK_VALID_EXT_APP;
}
