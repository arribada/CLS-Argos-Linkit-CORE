#ifndef UART_ASYNC_H
#define UART_ASYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include "sdk_errors.h"
#include "nrf_uarte.h"

/** @brief Types of libuarte driver events. */
typedef enum
{
    NRF_LIBUARTE_ASYNC_EVT_RX_DATA,  ///< Requested TX transfer completed.
    NRF_LIBUARTE_ASYNC_EVT_TX_DONE,  ///< Requested RX transfer completed.
    NRF_LIBUARTE_ASYNC_EVT_ERROR,    ///< Error reported by UARTE peripheral.
    NRF_LIBUARTE_ASYNC_EVT_OVERRUN_ERROR,  ///< Error reported by the driver.
    NRF_LIBUARTE_ASYNC_EVT_ALLOC_ERROR  ///< Error reported by the driver.
} nrf_libuarte_async_evt_type_t;

/** @brief Structure for libuarte async transfer completion event. */
typedef struct
{
    uint8_t  * p_data; ///< Pointer to memory used for transfer.
    size_t     length; ///< Number of bytes transfered.
} nrf_libuarte_async_data_t;

/** @brief Structu for software error event. */
typedef struct
{
    uint32_t overrun_length; ///< Number of bytes lost due to overrun.
} nrf_libuarte_async_overrun_err_evt_t;

/** @brief Structure for libuarte error event. */
typedef struct
{
    nrf_libuarte_async_evt_type_t type; ///< Event type.
    union {
        nrf_libuarte_async_data_t rxtx;                   ///< RXD/TXD data.
        uint8_t                   errorsrc;               ///< Error source.
        nrf_libuarte_async_overrun_err_evt_t overrun_err; ///< Overrun error data.
    } data;                                 ///< Union with data.
} nrf_libuarte_async_evt_t;

/**
 * @brief Interrupt event handler.
 *
 * @param[in] p_evt  Pointer to event structure. Event is allocated on the stack so it is available
 *                   only within the context of the event handler.
 */
typedef void (*nrf_libuarte_async_evt_handler_t)(void * context, nrf_libuarte_async_evt_t * p_evt);

/** @brief Structure for libuarte async configuration. */
typedef struct
{
} nrf_libuarte_async_config_t;

/**
 * @brief nrf_libuarte_async control block (placed in RAM).
 */
typedef struct {
} nrf_libuarte_async_ctrl_blk_t;

/**
 * @brief nrf_libuarte_async instance structure (placed in ROM).
 */
typedef struct {
    const nrf_uarte_t *p_libuarte;
} nrf_libuarte_async_t;


/**
 * @brief Function for initializing the libuarte async library.
 *
 * @param[in] p_libuarte   Libuarte_async instance.
 * @param[in] p_config     Pointer to the structure with initial configuration.
 * @param[in] evt_handler  Event handler provided by the user. Must not be NULL.
 * @param[in] context      User context passed to the event handler.
 *
 * @return NRF_SUCCESS when properly initialized. NRF_ERROR_INTERNAL otherwise.
 */
ret_code_t nrf_libuarte_async_init(const nrf_libuarte_async_t * const p_libuarte,
                                   nrf_libuarte_async_config_t const * p_config,
                                   nrf_libuarte_async_evt_handler_t    evt_handler,
                                   void * context);

/** @brief Function for uninitializing the libuarte async library.
 *
 * @param[in] p_libuarte   Libuarte_async instance.
 */
void nrf_libuarte_async_uninit(const nrf_libuarte_async_t * const p_libuarte);

/**
 * @brief Function for starting receiver.
 *
 * @param p_libuarte  Libuarte_async instance.
 */
void nrf_libuarte_async_start_rx(const nrf_libuarte_async_t * const p_libuarte);

/**
 * @brief Function for stopping receiver.
 *
 * @param p_libuarte  Libuarte_async instance.
 */
void nrf_libuarte_async_stop_rx(const nrf_libuarte_async_t * const p_libuarte);

/**
 * @brief Function for modifying the receive timeout.
 *        This disables/enables the receiver temporarily.
 *
 * @param p_libuarte  Libuarte_async instance.
 */
void nrf_libuarte_async_set_timeout(const nrf_libuarte_async_t * const p_libuarte, unsigned int timeout_us);

/**
 * @brief Function for deasserting RTS to pause the transmission.
 *
 * Flow control must be enabled.
 *
 * @param p_libuarte Libuarte_async instance.
 */
void nrf_libuarte_async_rts_clear(const nrf_libuarte_async_t * const p_libuarte);

/**
 * @brief Function for asserting RTS to restart the transmission.
 *
 * Flow control must be enabled.
 *
 * @param p_libuarte Libuarte_async instance.
 */
void nrf_libuarte_async_rts_set(const nrf_libuarte_async_t * const p_libuarte);

/**
 * @brief Function for sending data asynchronously over UARTE.
 *
 * @param[in] p_libuarte Libuarte_async instance.
 * @param[in] p_data  Pointer to data.
 * @param[in] length  Number of bytes to send. Maximum possible length is
 *                    dependent on the used SoC (see the MAXCNT register
 *                    description in the Product Specification). The library
 *                    checks it with assertion.
 *
 * @retval NRF_ERROR_BUSY      Data is transferring.
 * @retval NRF_ERROR_INTERNAL  Error during configuration.
 * @retval NRF_SUCCESS         Buffer set for sending.
 */
ret_code_t nrf_libuarte_async_tx(const nrf_libuarte_async_t * const p_libuarte,
                                 uint8_t * p_data, size_t length);

/**
 * @brief Function for deallocating received buffer data.
 *
 * @param[in] p_libuarte Libuarte_async instance.
 * @param[in] p_data  Pointer to data.
 * @param[in] length  Number of bytes to free.
 */
void nrf_libuarte_async_rx_free(const nrf_libuarte_async_t * const p_libuarte,
                                uint8_t * p_data, size_t length);

// Test injection
void nrf_libuarte_inject_event(nrf_libuarte_async_evt_t *evt);

/** @} */

#ifdef __cplusplus
}
#endif

#endif //UART_ASYNC_H
