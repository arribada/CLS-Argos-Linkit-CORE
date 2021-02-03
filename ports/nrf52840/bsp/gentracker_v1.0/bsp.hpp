#pragma once

#include <cstdint>
#include "sdk_config.h"
#include "nrfx_uarte.h"
#include "nrfx_qspi.h"
#include "nrfx_rtc.h"
#include "nrfx_spim.h"
#include "nrfx_gpiote.h"
#include "nrfx_saadc.h"
#include "nrf_gpio.h"

// Locate code in RAM
#define __RAMFUNC __attribute__ ((long_call, optimize("Os"), section (".ramfunc")))

// Logical device mappings to physical devices
#define RTC_DATE_TIME  BSP::RTC::RTC_1
#define RTC_TIMER      BSP::RTC::RTC_2
#define SPI_SATELLITE  BSP::SPI::SPI_2
#define BATTERY_ADC	   BSP::ADC::ADC_CHANNEL_0

namespace BSP
{
	///////////////////////////////// GPIO definitions ////////////////////////////////
	enum GPIO
	{
	    GPIO_POWER_CONTROL,
	    GPIO_DEBUG,
	    GPIO_SWS,
	    GPIO_SWS_EN,
	    GPIO_SLOW_SWS_RX,
	    GPIO_SLOW_SWS_SEND,
	    GPIO_WCHG_INTB,
	    GPIO_AG_PWR,
	    GPIO_G8_33,
	    GPIO_G16_33,
	    GPIO_SAT_RESET,
	    GPIO_SAT_EN,
	    GPIO_EXT1_GPIO1,
	    GPIO_EXT1_GPIO2,
	    GPIO_EXT1_GPIO3,
	    GPIO_GPS_EXT_INT,
	    GPIO_LED_GREEN,
	    GPIO_LED_RED,
	    GPIO_LED_BLUE,
	    GPIO_GPS_PWR_EN,
	    GPIO_INT1_SAT,
	    GPIO_INT2_SAT,
	    GPIO_REED_SW,
	    GPIO_INT1_AG,
	    GPIO_INT2_AG,
	    GPIO_FLASH_IO2,
	    GPIO_FLASH_IO3,
		GPIO_TOTAL_NUMBER
	};

	typedef struct
	{
		uint32_t             pin_number;
		nrf_gpio_pin_dir_t   dir;
		nrf_gpio_pin_input_t input;
		nrf_gpio_pin_pull_t  pull;
		nrf_gpio_pin_drive_t drive;
		nrf_gpio_pin_sense_t sense;
		nrfx_gpiote_in_config_t gpiote_in_config;
	} GPIO_InitTypeDefAndInst_t;

	extern const GPIO_InitTypeDefAndInst_t GPIO_Inits[GPIO_TOTAL_NUMBER];

	// Interrupt priorities (0, 1, 4  are reserved for the softdevice)
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

    ////////////////////////////////// UART definitions /////////////////////////////////

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

    ////////////////////////////////// QSPI definitions /////////////////////////////////

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

    ////////////////////////////////// RTC definitions /////////////////////////////////
    enum RTC
    {
    #if NRFX_CHECK(NRFX_RTC0_ENABLED)
        RTC_RESERVED, // Reserved by the softdevice
    #endif
    #if NRFX_CHECK(NRFX_RTC1_ENABLED)
        RTC_1, // Used by the app timer (there is likely scope to free this up by modifying the BLE connection handling)
    #endif
    #if NRFX_CHECK(NRFX_RTC2_ENABLED)
        RTC_2,
    #endif
        RTC_TOTAL_NUMBER
    };

    typedef struct
    {
        nrfx_rtc_t rtc;
        uint8_t irq_priority;
    } RTC_InitTypeDefAndInst_t;

    extern const RTC_InitTypeDefAndInst_t RTC_Inits[RTC_TOTAL_NUMBER];

    ////////////////////////////////// SPI definitions /////////////////////////////////

    enum SPI
    {
#if NRFX_SPIM0_ENABLED
    	SPI_0,
#endif
#if NRFX_SPIM1_ENABLED
		SPI_1,
#endif
#if NRFX_SPIM2_ENABLED
		SPI_2,
#endif
#if NRFX_SPIM3_ENABLED
		SPI_3,
#endif
		SPI_TOTAL_NUMBER
    };

    typedef struct
    {
        nrfx_spim_t spim;
        nrfx_spim_config_t spim_config;
    } SPI_InitTypeDefAndInst_t;

    extern const SPI_InitTypeDefAndInst_t SPI_Inits[SPI_TOTAL_NUMBER];

    ////////////////////////////////// ADC definitions /////////////////////////////////

    enum ADC
    {
        ADC_CHANNEL_0,
        ADC_TOTAL_CHANNELS
    };

    typedef struct
    {
    	nrfx_saadc_config_t saadc_config;
        nrf_saadc_channel_config_t saadc_config_channel_config[ADC_TOTAL_CHANNELS];
    } ADC_InitTypeDefAndInst_t;

    extern const ADC_InitTypeDefAndInst_t ADC_Inits;
}
