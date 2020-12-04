// This file redirects any NRF logging to stdout through the use of a custom nrf_log backend

#include <nrfx.h>
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_log_backend_serial.h"
#include "nrf_log_redirect.h"

#define NRF_LOG_BACKEND_UART_TEMP_BUFFER_SIZE (512)

static void serial_tx(void const * p_context, char const * p_buffer, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        printf("%c", p_buffer[i]);
}

static uint8_t m_string_buff[NRF_LOG_BACKEND_UART_TEMP_BUFFER_SIZE];

static void nrf_log_custom_backend_uart_put(nrf_log_backend_t const * p_backend,
                                     nrf_log_entry_t * p_msg)
{
    nrf_log_backend_serial_put(p_backend, p_msg, m_string_buff,
                               NRF_LOG_BACKEND_UART_TEMP_BUFFER_SIZE, serial_tx);
}

static void nrf_log_custom_backend_uart_flush(nrf_log_backend_t const * p_backend)
{
    fflush(stdout);
}

static void nrf_log_custom_backend_uart_panic_set(nrf_log_backend_t const * p_backend)
{
    // ?
}

static const nrf_log_backend_api_t nrf_log_backend_uart_api = {
        .put       = nrf_log_custom_backend_uart_put,
        .flush     = nrf_log_custom_backend_uart_flush,
        .panic_set = nrf_log_custom_backend_uart_panic_set,
};

static nrf_log_backend_cb_t nrf_log_backend_uart_cb = {
    .p_next = NULL,
    .id = NRF_LOG_BACKEND_INVALID_ID,
    .enabled = false,
};

static const nrf_log_backend_t nrf_log_backend_uart = {
    .p_api = &nrf_log_backend_uart_api,  //!< Pointer to interface.
    .p_ctx = NULL,  //!< User context.
    .p_name = "CUSTOM_UART", //!< Name of the backend.
    .p_cb = &nrf_log_backend_uart_cb,   //!< Pointer to the backend control block.
};

void nrf_log_redirect_init(void)
{
    nrf_log_init(NULL, LOG_TIMESTAMP_DEFAULT_FREQUENCY);
    int32_t backend_id = nrf_log_backend_add(&nrf_log_backend_uart, NRF_LOG_SEVERITY_DEBUG);
    ASSERT(backend_id >= 0);
    nrf_log_backend_enable(&nrf_log_backend_uart);
}
