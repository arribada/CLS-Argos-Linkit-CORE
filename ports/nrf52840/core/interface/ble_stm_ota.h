/**
 * Copyright (c) 2015 - 2020, Nordic Semiconductor ASA
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
 * @defgroup ble_stm_ota STM OTA Update Service
 * @{
 * @ingroup ble_sdk_srv
 *
 * @brief STM OTA Update Service
 *
 * @details This module implements the STM OTA Update Service (0000FE20-CC7A-482A-984A-7F2ED5B3E58F) comprising the following characteristics:
 *
 *          Base Address (0000FE22-CC7A-482A-984A-7F2ED5B3E58F) - 4 bytes
 *            Byte 0 - Actions:  0=>STOP all upload, 1=>START file upload, 2=>start user app upload, 7=>upload finished
 *                            :  8=>cancel upload
 *            Bytes 1-3: Address: 0xXXXXXX
 *
 *          File Upload End Status (0000FE23-CC7A-482A-984A-7F2ED5B3E58F)
 *            Byte 0 - Status: 0=>OK, 1=>Not OK
 *
 *          OTA Raw Data (0000FE24-CC7A-482A-984A-7F2ED5B3E58F)
 *            Bytes 0-19: Raw File Data
 *
 *          During initialization, the module adds the characteristics to the BLE stack database.
 *
 *          The application must supply an event handler for receiving events. Using this handler, the service notifies
 *          the application when OTA Update Service events arise.
 *
 *          The service also provides a function for letting the application notify the success/failure of
 *          an OTA update operation via the "File Upload End Status" characteristic.
 *
 * @note    The application must register this module as a BLE event observer using the
 *          NRF_SDH_BLE_OBSERVER macro. Example:
 *          @code
 *              ble_hids_t instance;
 *              NRF_SDH_BLE_OBSERVER(anything, BLE_STM_OTA_BLE_OBSERVER_PRIO,
 *                                   ble_stm_ota_on_ble_evt, &instance);
 *          @endcode
 */

#ifndef BLE_STM_OTA_H__
#define BLE_STM_OTA_H__

#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**@brief   Macro for defining a ble_stm_ota instance.
 *
 * @param   _name   Name of the instance.
 * @hideinitializer
 */
#define BLE_STM_OTA_DEF(_name)                                                                          \
static ble_stm_ota_t _name;                                                                         \
NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                                 \
                     BLE_STM_OTA_BLE_OBSERVER_PRIO,                                                 \
                     ble_stm_ota_on_ble_evt, &_name)

#define STM_OTA_UUID_BASE        {0x8F, 0xE5, 0xB3, 0xD5, 0x2E, 0x7F, 0x4A, 0x98, \
                              	  0x2A, 0x48, 0x7A, 0xCC, 0x00, 0x00, 0x00, 0x00}


#define STM_OTA_UUID_SERVICE     			0xFE20
#define STM_OTA_UUID_BASE_ADDRESS 			0xFE22
#define STM_OTA_UUID_FILE_UPLOAD_END_STATUS	0xFE23
#define STM_OTA_UUID_OTA_RAW_DATA			0xFE24

#define STM_OTA_FILE_UPLOAD_STATUS_OK		0
#define STM_OTA_FILE_UPLOAD_STATUS_NOT_OK	1

// Forward declaration of the ble_lbs_t type.
typedef struct ble_stm_ota_s ble_stm_ota_t;

typedef enum {
	STM_OTA_EVENT_ACTION,
	STM_OTA_EVENT_RAW_DATA
} ble_stm_ota_event_type_t;

typedef enum {
	STM_OTA_ACTION_STOP_ALL,
	STM_OTA_ACTION_START_WIRELESS,
	STM_OTA_ACTION_START_USER_APP,
	STM_OTA_ACTION_UPLOAD_FINISHED = 7,
	STM_OTA_ACTION_CANCEL
} ble_stm_ota_action_t;

typedef struct
{
	ble_stm_ota_event_type_t event_type;
	union {
		struct {
			ble_stm_ota_action_t action;
			uint32_t address;
		};
		struct {
			void * p_data;
			uint8_t length;
		};
	};
} ble_stm_ota_event_t;

typedef void (*ble_stm_ota_event_handler_t) (uint16_t conn_handle, ble_stm_ota_t * p_stm_ota, ble_stm_ota_event_t * p_event);

/** @brief STM OTA Service init structure. This structure contains all options and data needed for
 *        initialization of the service.*/
typedef struct
{
	ble_stm_ota_event_handler_t event_handler; /**< Event handler to be called when client writes one of the characteristics */
} ble_stm_ota_init_t;

/**@brief STM OTA Service structure. This structure contains various status information for the service. */
struct ble_stm_ota_s
{
    uint16_t                    service_handle;      /**< Handle of LED Button Service (as provided by the BLE stack). */
    ble_gatts_char_handles_t    base_address_char_handles;    /**< Handles related to the Base Address Characteristic. */
    ble_gatts_char_handles_t    file_upload_end_status_char_handles;    /**< Handles related to the File Upload End Status Characteristic. */
    ble_gatts_char_handles_t    ota_raw_data_char_handles; /**< Handles related to the OTA Raw Data Characteristic. */
    uint8_t                     uuid_type;           /**< UUID type for the STM OTA Service. */
    ble_stm_ota_event_handler_t event_handler;   /**< Event handler to be called when a Characteristic is written. */
};


/**@brief Function for initializing the STM OTA Service.
 *
 * @param[out] p_stm_ota  STM OTA Service structure. This structure must be supplied by
 *                        the application. It is initialized by this function and will later
 *                        be used to identify this particular service instance.
 * @param[in] p_stm_ota_init  Information needed to initialize the service.
 *
 * @retval NRF_SUCCESS If the service was initialized successfully. Otherwise, an error code is returned.
 */
uint32_t ble_stm_ota_init(ble_stm_ota_t * p_stm_ota, const ble_stm_ota_init_t * p_stm_ota_init);


/**@brief Function for handling the application's BLE stack events.
 *
 * @details This function handles all events from the BLE stack that are of interest to the LED Button Service.
 *
 * @param[in] p_ble_evt  Event received from the BLE stack.
 * @param[in] p_context  LED Button Service structure.
 */
void ble_stm_ota_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);


/**@brief Function for sending File Upload End Status.
 *
 ' @param[in] conn_handle   Handle of the peripheral connection to which the button state notification will be sent.
 * @param[in] p_lbs         STM OTA Button Service structure.
 * @param[in] status        Success/Failure status code
 *
 * @retval NRF_SUCCESS If the notification was sent successfully. Otherwise, an error code is returned.
 */
uint32_t ble_stm_ota_on_file_upload_end_status(uint16_t conn_handle, ble_stm_ota_t * p_stm_ota, uint8_t status);


#ifdef __cplusplus
}
#endif

#endif // BLE_STM_OTA_H__

/** @} */
