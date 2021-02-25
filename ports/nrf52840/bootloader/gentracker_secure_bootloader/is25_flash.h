#ifndef __IS25_FLASH_H_
#define __IS25_FLASH_H_

#include <stdint.h>

int is25_flash_init();
void is25_flash_deinit(void);
int is25_flash_read(uint32_t addr, uint8_t *buffer, uint32_t size);
int is25_flash_erase(uint32_t block);

#endif // __IS25_FLASH_H_
