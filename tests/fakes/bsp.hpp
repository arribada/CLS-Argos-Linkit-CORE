#pragma once

#include <cstdint>
#include "nrfx_rtc.h"
#include "nrfx_saadc.h"

#define RTC_TIMER      BSP::RTC::RTC_1

namespace BSP
{
    enum RTC
    {
        RTC_RESERVED, // Reserved by the softdevice
        RTC_1,
        RTC_2,
        RTC_TOTAL_NUMBER
    };

    typedef struct
    {
        nrfx_rtc_t rtc;
        uint8_t irq_priority;
    } RTC_InitTypeDefAndInst_t;

    extern const RTC_InitTypeDefAndInst_t RTC_Inits[RTC_TOTAL_NUMBER];

    enum GPIO
	{
    	GPIO_SWITCH,
		GPIO_POWER_CONTROL,
		GPIO_SLOW_SWS_SEND,
		GPIO_SLOW_SWS_RX,
		GPIO_TOTAL_NUMBER
	};

    typedef struct
    {
    	int pin_number;
    	void *gpiote_in_config;
    } GPIO_InitTypeDefAndInst_t;

    extern const GPIO_InitTypeDefAndInst_t GPIO_Inits[GPIO_TOTAL_NUMBER];

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
