#pragma once

#include <cstdint>
#include "sdk_config.h"
#include "nrfx_uarte.h"
#include "nrfx_qspi.h"

namespace BSP
{

    // Interrupt priorities (0, 1, 4  are reserved for the softdevice)
    static constexpr uint8_t INTERRUPT_PRIORITY_WATCHDOG  = 2;
    static constexpr uint8_t INTERRUPT_PRIORITY_RTC_1     = 2;
    static constexpr uint8_t INTERRUPT_PRIORITY_RTC_2     = 3;
    static constexpr uint8_t INTERRUPT_PRIORITY_UART_0    = 4;
    static constexpr uint8_t INTERRUPT_PRIORITY_UART_1    = 4;
    static constexpr uint8_t INTERRUPT_PRIORITY_I2C_0     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_I2C_1     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_0     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_1     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_2     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_SPI_3     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_QSPI_0    = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_0   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_1   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_2   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_3   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_TIMER_4   = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_0     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_1     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_2     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_PWM_3     = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_ADC       = 6;
    static constexpr uint8_t INTERRUPT_PRIORITY_RTC_0     = 6;

    enum UART
    {
    #if NRFX_UARTE0_ENABLED
        UART_0,
    #endif
    #if NRFX_UARTE1_ENABLED
        UART_1,
    #endif
        UART_TOTAL_NUMBER
    };

    struct UART_InitTypeDefAndInst
    {
        nrfx_uarte_t uarte;
        nrfx_uarte_config_t config;
    };

    extern const UART_InitTypeDefAndInst UART_Inits[UART_TOTAL_NUMBER];

    enum QSPI
    {
    #ifdef NRFX_QSPI_ENABLED
        QSPI_0,
    #endif
        QSPI_TOTAL_NUMBER
    };

    struct QSPI_InitTypeDefAndInst
    {
        nrfx_qspi_config_t config;
    };

    extern const QSPI_InitTypeDefAndInst QSPI_Inits[QSPI_TOTAL_NUMBER];
}