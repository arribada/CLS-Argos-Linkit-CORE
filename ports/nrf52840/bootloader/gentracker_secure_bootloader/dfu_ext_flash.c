#include <stdint.h>

#include "is25_flash.h"
#include "IS25LP128F.h"


void ext_flash_read(uint8_t *dst_addr, uint32_t src_addr, uint32_t length)
{
	is25_flash_read(src_addr / IS25_BLOCK_SIZE, src_addr % IS25_BLOCK_SIZE, dst_addr, length);
}
