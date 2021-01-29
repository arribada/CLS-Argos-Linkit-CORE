#ifndef __NRF_ERR_H_
#define __NRF_ERR_H_

#define NRFX_ERROR_BASE_NUM         0x0BAD0000
#define NRFX_ERROR_DRIVERS_BASE_NUM (NRFX_ERROR_BASE_NUM + 0x10000)

/** @brief Enumerated type for error codes. */
typedef enum
{
    NRFX_SUCCESS                    = (NRFX_ERROR_BASE_NUM + 0),  ///< Operation performed successfully.
    NRFX_ERROR_INTERNAL             = (NRFX_ERROR_BASE_NUM + 1),  ///< Internal error.
    NRFX_ERROR_NO_MEM               = (NRFX_ERROR_BASE_NUM + 2),  ///< No memory for operation.
    NRFX_ERROR_NOT_SUPPORTED        = (NRFX_ERROR_BASE_NUM + 3),  ///< Not supported.
    NRFX_ERROR_INVALID_PARAM        = (NRFX_ERROR_BASE_NUM + 4),  ///< Invalid parameter.
    NRFX_ERROR_INVALID_STATE        = (NRFX_ERROR_BASE_NUM + 5),  ///< Invalid state, operation disallowed in this state.
    NRFX_ERROR_INVALID_LENGTH       = (NRFX_ERROR_BASE_NUM + 6),  ///< Invalid length.
    NRFX_ERROR_TIMEOUT              = (NRFX_ERROR_BASE_NUM + 7),  ///< Operation timed out.
    NRFX_ERROR_FORBIDDEN            = (NRFX_ERROR_BASE_NUM + 8),  ///< Operation is forbidden.
    NRFX_ERROR_NULL                 = (NRFX_ERROR_BASE_NUM + 9),  ///< Null pointer.
    NRFX_ERROR_INVALID_ADDR         = (NRFX_ERROR_BASE_NUM + 10), ///< Bad memory address.
    NRFX_ERROR_BUSY                 = (NRFX_ERROR_BASE_NUM + 11), ///< Busy.
    NRFX_ERROR_ALREADY_INITIALIZED  = (NRFX_ERROR_BASE_NUM + 12), ///< Module already initialized.

    NRFX_ERROR_DRV_TWI_ERR_OVERRUN  = (NRFX_ERROR_DRIVERS_BASE_NUM + 0), ///< TWI error: Overrun.
    NRFX_ERROR_DRV_TWI_ERR_ANACK    = (NRFX_ERROR_DRIVERS_BASE_NUM + 1), ///< TWI error: Address not acknowledged.
    NRFX_ERROR_DRV_TWI_ERR_DNACK    = (NRFX_ERROR_DRIVERS_BASE_NUM + 2)  ///< TWI error: Data not acknowledged.
} nrfx_err_t;

#endif // __NRF_ERR_H_
