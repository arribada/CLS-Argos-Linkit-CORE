#include "dfu.hpp"

#include "nrf_dfu_settings.h"
#include "nrf_dfu_types.h"

void DFU::initialise()
{
	nrf_dfu_settings_init(false);
}

void DFU::write_ext_flash_dfu_settings(uint32_t image_size, uint32_t crc)
{
	s_dfu_settings.bank_1.image_size = image_size;
	s_dfu_settings.bank_1.image_crc = crc;
	s_dfu_settings.bank_1.bank_code = NRF_DFU_BANK_VALID_EXT_APP;
	nrf_dfu_settings_write_and_backup(nullptr);
}
