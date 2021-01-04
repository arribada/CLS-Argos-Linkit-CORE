#include "bsp.hpp"
#include "nrf_gpio.h"

namespace BSP
{
    /////////////////////////////////// UART definitions ////////////////////////////////

    // Supported baudrates:
    // NRF_UARTE_BAUDRATE_1200
    // NRF_UARTE_BAUDRATE_2400
    // NRF_UARTE_BAUDRATE_4800
    // NRF_UARTE_BAUDRATE_9600
    // NRF_UARTE_BAUDRATE_14400
    // NRF_UARTE_BAUDRATE_19200
    // NRF_UARTE_BAUDRATE_28800
    // NRF_UARTE_BAUDRATE_31250
    // NRF_UARTE_BAUDRATE_38400
    // NRF_UARTE_BAUDRATE_56000
    // NRF_UARTE_BAUDRATE_57600
    // NRF_UARTE_BAUDRATE_76800
    // NRF_UARTE_BAUDRATE_115200
    // NRF_UARTE_BAUDRATE_230400
    // NRF_UARTE_BAUDRATE_250000
    // NRF_UARTE_BAUDRATE_460800
    // NRF_UARTE_BAUDRATE_921600
    // NRF_UARTE_BAUDRATE_1000000

    const UART_InitTypeDefAndInst UART_Inits[UART_TOTAL_NUMBER] =
    {
    #if NRFX_UARTE0_ENABLED
        {
            .uarte = NRFX_UARTE_INSTANCE(0),
            {
                .pseltxd = NRF_GPIO_PIN_MAP(1, 9),
                .pselrxd = NRF_GPIO_PIN_MAP(1, 8),
                .pselcts = NRF_UARTE_PSEL_DISCONNECTED,
                .pselrts = NRF_UARTE_PSEL_DISCONNECTED,
                .p_context = NULL, // Context passed to interrupt handler
                .hwfc = NRF_UARTE_HWFC_DISABLED,
                .parity = NRF_UARTE_PARITY_EXCLUDED,
                .baudrate = NRF_UARTE_BAUDRATE_460800, // See table above
                .interrupt_priority = INTERRUPT_PRIORITY_UART_0,
            }
        },
    #endif
    #if NRFX_UARTE1_ENABLED
        {
            .uarte = NRFX_UARTE_INSTANCE(1),
            {
                .pseltxd = NRF_GPIO_PIN_MAP(0, 11),
                .pselrxd = NRF_GPIO_PIN_MAP(0, 14),
                .pselcts = NRF_UARTE_PSEL_DISCONNECTED,
                .pselrts = NRF_UARTE_PSEL_DISCONNECTED,
                .p_context = NULL, // Context passed to interrupt handler
                .hwfc = NRF_UARTE_HWFC_DISABLED,
                .parity = NRF_UARTE_PARITY_EXCLUDED,
                .baudrate = NRF_UARTE_BAUDRATE_460800, // See table above
                .interrupt_priority = INTERRUPT_PRIORITY_UART_1,
            }
        }
    #endif
    };

    /////////////////////////////////// QSPI definitions ////////////////////////////////

    // Supported frequencies:
    // NRF_QSPI_FREQ_32MDIV1    32MHz/1
    // NRF_QSPI_FREQ_32MDIV2    32MHz/2
    // NRF_QSPI_FREQ_32MDIV3    32MHz/3
    // NRF_QSPI_FREQ_32MDIV4    32MHz/4
    // NRF_QSPI_FREQ_32MDIV5    32MHz/5
    // NRF_QSPI_FREQ_32MDIV6    32MHz/6
    // NRF_QSPI_FREQ_32MDIV7    32MHz/7
    // NRF_QSPI_FREQ_32MDIV8    32MHz/8
    // NRF_QSPI_FREQ_32MDIV9    32MHz/9
    // NRF_QSPI_FREQ_32MDIV10   32MHz/10
    // NRF_QSPI_FREQ_32MDIV11   32MHz/11
    // NRF_QSPI_FREQ_32MDIV12   32MHz/12
    // NRF_QSPI_FREQ_32MDIV13   32MHz/13
    // NRF_QSPI_FREQ_32MDIV14   32MHz/14
    // NRF_QSPI_FREQ_32MDIV15   32MHz/15
    // NRF_QSPI_FREQ_32MDIV16   32MHz/16

    const QSPI_InitTypeDefAndInst QSPI_Inits[QSPI_TOTAL_NUMBER] =
    {
    #ifdef NRFX_QSPI_ENABLED
        {
            {
                .xip_offset = 0, // Address offset in the external memory for Execute in Place operation
                {
                    .sck_pin = NRF_GPIO_PIN_MAP(0, 19),
                    .csn_pin = NRF_GPIO_PIN_MAP(0, 24),
                    .io0_pin = NRF_GPIO_PIN_MAP(0, 21),
                    .io1_pin = NRF_GPIO_PIN_MAP(0, 23),
                    .io2_pin = NRF_GPIO_PIN_MAP(0, 22),
                    .io3_pin = NRF_GPIO_PIN_MAP(1,  0),
                },
                {
                    .readoc = NRF_QSPI_READOC_READ4IO, // Number of data lines and opcode used for reading
                    .writeoc = NRF_QSPI_WRITEOC_PP4O,  // Number of data lines and opcode used for writing
                    .addrmode = NRF_QSPI_ADDRMODE_24BIT,
                    .dpmconfig = false, // Deep power-down mode enable
                },
                {
                    .sck_delay = 0, // SCK delay in units of 62.5 ns  <0-255>
                    .dpmen = false, // Deep power-down mode enable
                    .spi_mode = NRF_QSPI_MODE_0,
                    .sck_freq = NRF_QSPI_FREQ_32MDIV1, // See table above
                },
                .irq_priority = INTERRUPT_PRIORITY_QSPI_0
            }
        }
    #endif
    };

}