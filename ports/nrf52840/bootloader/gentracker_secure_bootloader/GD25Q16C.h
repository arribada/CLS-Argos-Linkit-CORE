#ifndef __GD25Q16C_H_
#define __GD25Q16C_H_

#include <stdint.h>

#define GD25_BLOCK_COUNT   (4096)
#define GD25_BLOCK_SIZE    (4*1024)
#define GD25_PAGE_SIZE     (256)


static const uint32_t PAGE_SIZE = 256;

static const uint8_t MANUFACTURER_ID = 0xC8;
static const uint8_t MEMORY_TYPE_ID  = 0x40;
static const uint8_t CAPACITY_ID     = 0x15;

/* SPI flash comments */
static const uint8_t PP            = 0x02; // Page program -
static const uint8_t QPP           = 0x32; // Quad Page program -
static const uint8_t READ          = 0x03; // Normal read operation -
static const uint8_t QFREAD        = 0x6B; // Quad Fast read operation -
static const uint8_t WREN          = 0x06; // Write enable -
static const uint8_t WRDI          = 0x04; // Write disable -
//static const uint8_t RDJDIDQ       = 0xAF; // Read JEDEC ID QPI mode
static const uint8_t RDJDID        = 0x9F; // Read JEDEC ID SPI mode
static const uint8_t RDSRL         = 0x05; // Read Status register LOWER part 
static const uint8_t RDSRU         = 0x35; // Read Status register UPPER part 
//static const uint8_t QPIEN         = 0x35; // Enable QSPI mode
static const uint8_t WRSR          = 0x01;
//static const uint8_t RDERP         = 0x81; // Read extended read parameters
//static const uint8_t RDBR          = 0x16; // Read bank address register
//static const uint8_t SERPNV        = 0x85; // Set Extended Read Parameters (Non-Volatile)
//static const uint8_t SERPV         = 0x83; // Set Extended Read Parameters (Volatile)
static const uint8_t BER64K        = 0xD8; // Block erase 64Kbyte
static const uint8_t DP    = 0xB9; // Power down the device
static const uint8_t RDPD      = 0xAB; // Wake the device from a powered down state

/* SPI flash status bits */
static const uint16_t STATUS_WIP   = 1 << 0; // Write in progress
static const uint16_t STATUS_WEL   = 1 << 1; // Write enable latch
static const uint16_t STATUS_BP0   = 1 << 2; // Block protection bit
static const uint16_t STATUS_BP1   = 1 << 3; // Block protection bit
static const uint16_t STATUS_BP2   = 1 << 4; // Block protection bit
static const uint16_t STATUS_BP3   = 1 << 5; // Block protection bit
static const uint16_t STATUS_BP4   = 1 << 6; // Block protection bit
static const uint16_t STATUS_SRP0  = 1 << 7; // Status registe protection bit 0
static const uint16_t STATUS_SRP1  = 1 << 8; // Status registe protection bit 1
static const uint16_t STATUS_QE    = 1 << 9; // Quad enable bit
static const uint16_t STATUS_LB    = 1 << 10; // One Time Program bit 
static const uint16_t STATUS_HPF    = 1 << 13; // High Performance flag 
static const uint16_t STATUS_CMP    = 1 << 14; // 
static const uint16_t STATUS_SUS    = 1 << 15; // Suspend bit

#endif // __GD25Q16C_H_
