#pragma once

#include <cstdint>

namespace IS25LP128F
{
    
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
    constexpr uint8_t POWER_DOWN    = 0xB9; // Power down the device
    constexpr uint8_t POWER_UP      = 0xAB; // Wake the device from a powered down state

}