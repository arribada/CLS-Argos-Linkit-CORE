#include <stdbool.h>
#include "nrf_libuarte_async.h"
#include "debug.hpp"

static nrf_libuarte_async_evt_handler_t m_evt_callback;
static void *m_context;
static bool m_is_rx_enabled = false;

ret_code_t nrf_libuarte_async_init(const nrf_libuarte_async_t * const p_libuarte,
                                   nrf_libuarte_async_config_t const * p_config,
                                   nrf_libuarte_async_evt_handler_t    evt_handler,
                                   void * context) {
    (void)p_libuarte;
    (void)p_config;
    m_evt_callback = evt_handler;
    m_context = context;
    DEBUG_TRACE("nrf_libuarte_async_init");
    return 0;
}

void nrf_libuarte_async_uninit(const nrf_libuarte_async_t * const p_libuarte) {
    (void)p_libuarte;
    DEBUG_TRACE("nrf_libuarte_async_uninit");
}

void nrf_libuarte_async_start_rx(const nrf_libuarte_async_t * const p_libuarte) {
    (void)p_libuarte;
    m_is_rx_enabled = true;
    DEBUG_TRACE("nrf_libuarte_async_start_rx");
}

void nrf_libuarte_async_stop_rx(const nrf_libuarte_async_t * const p_libuarte) {
    (void)p_libuarte;
    m_is_rx_enabled = false;
    DEBUG_TRACE("nrf_libuarte_async_stop_rx");
}

void nrf_libuarte_async_set_timeout(const nrf_libuarte_async_t * const p_libuarte, unsigned int timeout_us) {
    (void)p_libuarte;
    (void)timeout_us;
    DEBUG_TRACE("nrf_libuarte_async_set_timeout");
}

ret_code_t nrf_libuarte_async_tx(const nrf_libuarte_async_t * const p_libuarte,
                                 uint8_t * p_data, size_t length) {
    (void)p_libuarte;
    (void)p_data;
    (void)length;
    DEBUG_TRACE("nrf_libuarte_async_tx");
    nrf_libuarte_async_evt_t evt;
    evt.type = NRF_LIBUARTE_ASYNC_EVT_TX_DONE;
    m_evt_callback(m_context, &evt);
    return 0;
}

void nrf_libuarte_async_rx_free(const nrf_libuarte_async_t * const p_libuarte,
                                uint8_t * p_data, size_t length) {
    (void)p_libuarte;
    (void)p_data;
    (void)length;
    DEBUG_TRACE("nrf_libuarte_async_rx_free");
}

void nrf_libuarte_inject_event(nrf_libuarte_async_evt_t *evt) {
    if (m_is_rx_enabled)
        m_evt_callback(m_context, evt);
}
