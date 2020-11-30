#include "bsp.hpp"

namespace BSP
{

    QSPI_InitTypeDefAndInst_t QSPI_Inits[QSPI_TOTAL_NUMBER] =
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
                    .sck_freq = 15, // Frequency divider (see table above)
                },
                .irq_priority = INTERRUPT_PRIORITY_QSPI_0
            }
        }
    #endif
    };

}