/**
 * Copyright (c) 2016 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup bootloader_secure_ble main.c
 * @{
 * @ingroup dfu_bootloader_api
 * @brief Bootloader project main file for secure DFU.
 *
 */

#include <stdint.h>
#include "boards.h"
#include "nrf_mbr.h"
#include "nrf_bootloader.h"
#include "nrf_bootloader_app_start.h"
#include "nrf_bootloader_dfu_timers.h"
#include "nrf_dfu.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "app_error.h"
#include "app_error_weak.h"
#include "nrf_bootloader_info.h"
#include "nrf_delay.h"
#include "nrfx_uarte.h"
#include "nrf_log_redirect.h"

static void on_error(void)
{
    NRF_LOG_FINAL_FLUSH();

#if NRF_MODULE_ENABLED(NRF_LOG_BACKEND_RTT)
    // To allow the buffer to be flushed by the host.
    nrf_delay_ms(100);
#endif
#ifdef NRF_DFU_DEBUG_VERSION
    NRF_BREAKPOINT_COND;
#endif
    NVIC_SystemReset();
}

static void led_yellow(void)
{
    bsp_board_led_off(BSP_BOARD_LED_0);
    bsp_board_led_off(BSP_BOARD_LED_1);
    bsp_board_led_on(BSP_BOARD_LED_2);
}

static void led_green(void)
{
    bsp_board_led_on(BSP_BOARD_LED_0);
    bsp_board_led_off(BSP_BOARD_LED_1);
    bsp_board_led_on(BSP_BOARD_LED_2);
}

static void led_red(void)
{
    bsp_board_led_off(BSP_BOARD_LED_0);
    bsp_board_led_on(BSP_BOARD_LED_1);
    bsp_board_led_on(BSP_BOARD_LED_2);
}

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    NRF_LOG_ERROR("%s:%d", p_file_name, line_num);
    on_error();
}


void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    NRF_LOG_ERROR("Received a fault! id: 0x%08x, pc: 0x%08x, info: 0x%08x", id, pc, info);
    on_error();
}


void app_error_handler_bare(uint32_t error_code)
{
    NRF_LOG_ERROR("Received an error: 0x%08x!", error_code);
    on_error();
}

/**
 * @brief Function notifies certain events in DFU process.
 */
static void dfu_observer(nrf_dfu_evt_type_t evt_type)
{
    switch (evt_type)
    {
        case NRF_DFU_EVT_DFU_FAILED:
        case NRF_DFU_EVT_DFU_ABORTED:
        	led_red();
        	break;
        case NRF_DFU_EVT_DFU_INITIALIZED:
        	led_yellow();
            break;
        case NRF_DFU_EVT_TRANSPORT_ACTIVATED:
        	led_green();
            break;
        case NRF_DFU_EVT_DFU_STARTED:
            break;
        case NRF_DFU_EVT_DFU_COMPLETED:
            break;
        default:
            break;
    }
}

typedef struct
{
    nrfx_uarte_t uarte;
    nrfx_uarte_config_t config;
} UART_InitTypeDefAndInst;

static const UART_InitTypeDefAndInst UART_Inits[] =
{
	{
		.uarte = NRFX_UARTE_INSTANCE(1),
		.config = {
			.pseltxd = NRF_GPIO_PIN_MAP(0, 11),
			.pselrxd = NRF_GPIO_PIN_MAP(0, 14),
			.pselcts = NRF_UARTE_PSEL_DISCONNECTED,
			.pselrts = NRF_UARTE_PSEL_DISCONNECTED,
			.p_context = NULL, // Context passed to interrupt handler
			.hwfc = NRF_UARTE_HWFC_DISABLED,
			.parity = NRF_UARTE_PARITY_EXCLUDED,
			.baudrate = NRF_UARTE_BAUDRATE_460800, // See table above
			.interrupt_priority = 4,
		}
	}
};

// Redirect printf output to debug UART
// We have to define this as extern "C" as we are overriding a weak C function
int _write(int file, char *ptr, int len)
{
	nrfx_uarte_tx(&UART_Inits[0].uarte, (const uint8_t *)ptr, len);
	return len;
}

void ext_flash_update(void);

/**@brief Function for application main entry. */
int main(void)
{
    uint32_t ret_val;

    // Initialise LEDs
    bsp_board_init(BSP_INIT_LEDS);

    // Power control on
    bsp_board_led_on(BSP_BOARD_LED_3);

    // Must happen before flash protection is applied, since it edits a protected page.
    nrf_bootloader_mbr_addrs_populate();

    // Protect MBR and bootloader code from being overwritten.
    ret_val = nrf_bootloader_flash_protect(0, MBR_SIZE);
    APP_ERROR_CHECK(ret_val);
    ret_val = nrf_bootloader_flash_protect(BOOTLOADER_START_ADDR, BOOTLOADER_SIZE);
    APP_ERROR_CHECK(ret_val);

    //(void) NRF_LOG_INIT(nrf_bootloader_dfu_timer_counter_get);
    //NRF_LOG_DEFAULT_BACKENDS_INIT();

    // Initialise UART and logging
	nrfx_uarte_init(&UART_Inits[0].uarte, &UART_Inits[0].config, NULL);
    setvbuf(stdout, NULL, _IONBF, 0);
	nrf_log_redirect_init();

    NRF_LOG_INFO("Inside main");

    // Check for and apply any external flash updates before entering main bootloader
    ext_flash_update();

    ret_val = nrf_bootloader_init(dfu_observer);
    APP_ERROR_CHECK(ret_val);

    NRF_LOG_FLUSH();

    NRF_LOG_ERROR("After main, should never be reached.");
    NRF_LOG_FLUSH();

    APP_ERROR_CHECK_BOOL(false);
}

/**
 * @}
 */
