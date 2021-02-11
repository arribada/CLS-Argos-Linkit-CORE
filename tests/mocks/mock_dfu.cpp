#include "dfu.hpp"

#include "CppUTestExt/MockSupport.h"

void DFU::initialise()
{
	mock().actualCall("initialise");
}

void DFU::write_ext_flash_dfu_settings(uint32_t image_size, uint32_t crc)
{
	mock().actualCall("write_ext_flash_dfu_settings").withParameter("image_size", image_size).withParameter("crc", crc);
}
