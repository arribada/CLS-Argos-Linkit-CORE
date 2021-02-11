#pragma once

#include <stdint.h>

class DFU {
public:
	static void initialise();
	static void write_ext_flash_dfu_settings(uint32_t image_size, uint32_t crc);
};
