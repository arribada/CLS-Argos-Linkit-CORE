#pragma once

#include <cstdint>

namespace IS25LP128F
{
    constexpr size_t PAGE_SIZE = 256;

    constexpr uint8_t MANUFACTURER_ID = 0x9D;
    constexpr uint8_t MEMORY_TYPE_ID  = 0x60;
    constexpr uint8_t CAPACITY_ID     = 0x18;
    
    /* SPI flash comments */
    constexpr uint8_t PP            = 0x02; // Page program
    constexpr uint8_t READ          = 0x03; // Normal read operation
    constexpr uint8_t WREN          = 0x06; // Write enable
    constexpr uint8_t WRDI          = 0x04;
    constexpr uint8_t RDJDIDQ       = 0xAF; // Read JEDEC ID QPI mode
    constexpr uint8_t RDJDID        = 0x9F; // Read JEDEC ID SPI mode
    constexpr uint8_t RDSR          = 0x05;
    constexpr uint8_t QPIEN         = 0x35; // Enable QSPI mode
    constexpr uint8_t WRSR          = 0x01;
    constexpr uint8_t RDERP         = 0x81; // Read extended read parameters
    constexpr uint8_t RDBR          = 0x16; // Read bank address register
    constexpr uint8_t SERPNV        = 0x85; // Set Extended Read Parameters (Non-Volatile)
    constexpr uint8_t SERPV         = 0x83; // Set Extended Read Parameters (Volatile)
    constexpr uint8_t BER64K        = 0xD8; // Block erase 64Kbyte
    constexpr uint8_t DP            = 0xB9; // Power down the device
    constexpr uint8_t RDPD          = 0xAB; // Wake the device from a powered down state

    /* SPI flash status bits */
    constexpr uint8_t STATUS_WIP   = 1 << 0; // Write in progress
    constexpr uint8_t STATUS_WEL   = 1 << 1; // Write enable latch
    constexpr uint8_t STATUS_BP0   = 1 << 2; // Block protection bit
    constexpr uint8_t STATUS_BP1   = 1 << 3; // Block protection bit
    constexpr uint8_t STATUS_BP2   = 1 << 4; // Block protection bit
    constexpr uint8_t STATUS_BP3   = 1 << 5; // Block protection bit
    constexpr uint8_t STATUS_QE    = 1 << 6; // Quad enable bit
    constexpr uint8_t STATUS_SRWD  = 1 << 7; // Status Register Write Disable
}