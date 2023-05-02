#ifndef SDK_ERRORS_H__
#define SDK_ERRORS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NRF_SUCCESS          0

/**
 * @brief API Result.
 *
 * @details Indicates success or failure of an API procedure. In case of failure, a comprehensive
 *          error code indicating cause or reason for failure is provided.
 *
 *          Though called an API result, it could used in Asynchronous notifications callback along
 *          with asynchronous callback as event result. This mechanism is employed when an event
 *          marks the end of procedure initiated using API. API result, in this case, will only be
 *          an indicative of whether the procedure has been requested successfully.
 */
typedef uint32_t ret_code_t;

/** @} */
/** @} */

#ifdef __cplusplus
}
#endif

#endif // SDK_ERRORS_H__
