/**
 * Copyright (c) 2013 - 2020, Nordic Semiconductor ASA
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
#include <stdio.h>
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_STM_OTA)
#include "ble_srv_common.h"
#include "ble_stm_ota.h"


/**@brief Function for handling the Write event.
 *
 * @param[in] p_stm_ota  STM OTA Service structure.
 * @param[in] p_ble_evt  Event received from the BLE stack.
 */
static void on_write(ble_stm_ota_t * p_stm_ota, ble_evt_t const * p_ble_evt)
{
	ble_stm_ota_event_t event;
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if ((p_evt_write->handle == p_stm_ota->base_address_char_handles.value_handle)
        && (p_evt_write->len == 4)
        && (p_stm_ota->event_handler != NULL))
    {
    	event.event_type = STM_OTA_EVENT_ACTION;
    	event.action = (ble_stm_ota_action_t)p_evt_write->data[0];
    	event.address = p_evt_write->data[1] |
    			p_evt_write->data[2] << 8 |
				p_evt_write->data[3] << 16;
    	p_stm_ota->event_handler(p_ble_evt->evt.gap_evt.conn_handle, p_stm_ota, &event);
    }
    else if ((p_evt_write->handle == p_stm_ota->ota_raw_data_char_handles.value_handle)
            && (p_stm_ota->event_handler != NULL))
    {
    	event.event_type = STM_OTA_EVENT_RAW_DATA;
    	event.p_data = (void *)p_evt_write->data;
    	event.length = p_evt_write->len;
    	p_stm_ota->event_handler(p_ble_evt->evt.gap_evt.conn_handle, p_stm_ota, &event);
    }
}


void ble_stm_ota_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
	ble_stm_ota_t * p_stm_ota = (ble_stm_ota_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTS_EVT_WRITE:
            on_write(p_stm_ota, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_stm_ota_init(ble_stm_ota_t * p_stm_ota, const ble_stm_ota_init_t * p_stm_ota_init)
{
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    ble_add_char_params_t add_char_params;

    // Initialize service structure.
    p_stm_ota->event_handler = p_stm_ota_init->event_handler;

    // Add service.
    ble_uuid128_t base_uuid = {STM_OTA_UUID_BASE};
    err_code = sd_ble_uuid_vs_add(&base_uuid, &p_stm_ota->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_stm_ota->uuid_type;
    ble_uuid.uuid = STM_OTA_UUID_SERVICE;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_stm_ota->service_handle);
    VERIFY_SUCCESS(err_code);

    // Add Base Address characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = STM_OTA_UUID_BASE_ADDRESS;
    add_char_params.uuid_type         = p_stm_ota->uuid_type;
    add_char_params.init_len          = 0;
    add_char_params.max_len           = 4;
    add_char_params.is_var_len        = false;
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.write  = 1;
    add_char_params.char_props.notify = 0;
    add_char_params.p_init_value      = NULL;

    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;

    err_code = characteristic_add(p_stm_ota->service_handle,
                                  &add_char_params,
                                  &p_stm_ota->base_address_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add File Upload Status characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid             = STM_OTA_UUID_FILE_UPLOAD_STATUS;
    add_char_params.uuid_type        = p_stm_ota->uuid_type;
    add_char_params.init_len         = 0;
    add_char_params.is_var_len       = false;
    add_char_params.max_len          = 3;
    add_char_params.char_props.write  = 0;
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.notify = 1;
    add_char_params.p_init_value      = NULL;

    add_char_params.read_access  = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    err_code = characteristic_add(p_stm_ota->service_handle, &add_char_params, &p_stm_ota->file_upload_end_status_char_handles);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add Raw Data characteristic.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = STM_OTA_UUID_OTA_RAW_DATA;
    add_char_params.uuid_type         = p_stm_ota->uuid_type;
    add_char_params.init_len          = 0;
    add_char_params.is_var_len        = true;
    add_char_params.max_len           = 240;
    add_char_params.char_props.read   = 0;
    add_char_params.char_props.write  = 1;
    add_char_params.char_props.notify = 0;
    add_char_params.char_props.write_wo_resp = 1;
    add_char_params.p_init_value      = NULL;

    add_char_params.write_access = SEC_OPEN;

    return characteristic_add(p_stm_ota->service_handle,
                              &add_char_params,
                              &p_stm_ota->ota_raw_data_char_handles);
}

uint32_t ble_stm_ota_on_file_upload_status(uint16_t conn_handle, ble_stm_ota_t * p_stm_ota, uint8_t status[3])
{
    ble_gatts_hvx_params_t params;
    uint16_t len = 3;

    memset(&params, 0, sizeof(params));
    params.type   = BLE_GATT_HVX_NOTIFICATION;
    params.handle = p_stm_ota->file_upload_end_status_char_handles.value_handle;
    params.p_data = status;
    params.p_len  = &len;

    return sd_ble_gatts_hvx(conn_handle, &params);
}

#endif // NRF_MODULE_ENABLED(BLE_LBS)
