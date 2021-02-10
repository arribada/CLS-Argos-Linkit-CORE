#include "ble_interface.hpp"

#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "nrf_log.h"
#include "ble_stm_ota.h"
#include "error.hpp"

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "GenTracker"                                /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */
#define STM_OTA_SERVICE_UUID_TYPE       BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the STM OTA Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */

#define APP_ADV_DURATION                18000                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(75, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */


/**< BLE NUS service instance. */
// These next few lines are equivalent to BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT) but is necessary for C++ compilation
BLE_LINK_CTX_MANAGER_DEF(m_nus_link_ctx_storage, (1), sizeof(ble_nus_client_context_t));
static ble_nus_t m_nus = {

    .uuid_type = 0,
    .service_handle = 0,
    .tx_handles = {0, 0, 0, 0},
    .rx_handles = {0, 0, 0, 0},
    .p_link_ctx_storage = &m_nus_link_ctx_storage,
    .data_handler = nullptr,
};
NRF_SDH_BLE_OBSERVER(m_nus_obs, BLE_NUS_BLE_OBSERVER_PRIO, ble_nus_on_ble_evt, &m_nus);

BLE_STM_OTA_DEF(m_stm_ota);

NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE},
#if 0  // FIXME: adding this UUID violates the max 31 bytes advertising packet size,
	   // so we can't advertise the OTA UUID in conjunction with the device name and NUS UUID
    {STM_OTA_UUID_SERVICE, STM_OTA_SERVICE_UUID_TYPE},
#endif
};

void BleInterface::init()
{
    m_carriage_return_received = false;
    m_receive_buffer_len = 0;
    m_is_first_ota_packet = false;

    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
}

void BleInterface::start(std::function<int(BLEServiceEvent& event)> on_event)
{
    m_on_event = on_event;

    advertising_start();
}

void BleInterface::stop()
{
    advertising_stop();

    // Disconnect if we were connected
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        NRF_LOG_DEBUG("Disconnecting...");

        ret_code_t err_code;
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);

        if (err_code != NRF_SUCCESS)
            NRF_LOG_ERROR("sd_ble_gap_disconnect() failed: 0x%08lX", err_code);
    }

    // Discard any received data
    m_carriage_return_received = false;
    m_receive_buffer_len = 0;
}

std::string BleInterface::read_line()
{
    std::string str;

    if (m_carriage_return_received)
    {
        for (uint32_t i = 0; i < m_receive_buffer_len; ++i)
            str.push_back(m_receive_buffer[i]);

        m_receive_buffer_len = 0;
        m_carriage_return_received = false;
    }
    
    return str;
}

void BleInterface::write(std::string str)
{
    uint32_t err_code;

    size_t length = str.length();

    // WARN: Unfortunately the ble_nus_data_send() library incorrectly requires a non-const data pointer
    // By examining the code we can see this is incorrect and at least for SDK v17.0.2 it is safe to cast away the const
    uint8_t * buffer = reinterpret_cast<uint8_t *>(const_cast<char *>(str.c_str()));

    do
    {
        uint16_t transfer_length = std::min(length, static_cast<size_t>(BLE_NUS_MAX_DATA_LEN));
        
        // WARN: Unfortunately the ble_nus_data_send() library incorrectly requires a non-const data pointer
        // By examining the code we can see this is incorrect and at least for SDK v17.0.2 it is safe to cast away the const
        err_code = ble_nus_data_send(&m_nus, buffer, &transfer_length, m_conn_handle);
        if ((err_code != NRF_ERROR_INVALID_STATE) &&
            (err_code != NRF_ERROR_RESOURCES) &&
            (err_code != NRF_ERROR_NOT_FOUND))
        {
            APP_ERROR_CHECK(err_code);
        }

        if (err_code == NRF_SUCCESS)
        {
            length -= transfer_length;
            buffer += transfer_length;
        }

    } while (length || (err_code == NRF_ERROR_RESOURCES));
}

void BleInterface::ble_stack_init()
{
    ret_code_t err_code;
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, static_ble_evt_handler, NULL);
}

void BleInterface::gap_params_init()
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

void BleInterface::gatt_init()
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, static_gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}

void BleInterface::services_init()
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    ble_stm_ota_init_t stm_ota_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = static_nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = static_nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);

    // Initialize STM OTA.
    memset(&m_stm_ota, 0, sizeof(m_stm_ota));

    stm_ota_init.event_handler = static_stm_ota_data_handler;

    err_code = ble_stm_ota_init(&m_stm_ota, &stm_ota_init);
    APP_ERROR_CHECK(err_code);
}

void BleInterface::advertising_init()
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.evt_handler = static_on_adv_evt;

    printf("ble_advertising_init\n");
    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

void BleInterface::advertising_start()
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

void BleInterface::advertising_stop()
{
    sd_ble_gap_adv_stop(m_advertising.adv_handle);
}

void BleInterface::ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            //err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            //APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            if (m_on_event) {
            	BLEServiceEvent e;
            	e.event_type = BLEServiceEventType::CONNECTED;
                (void)m_on_event(e);
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected for reason 0x%02X", p_ble_evt->evt.gap_evt.params.disconnected.reason);
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            if (m_on_event) {
            	BLEServiceEvent e;
            	e.event_type = BLEServiceEventType::DISCONNECTED;
                (void)m_on_event(e);
            }
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
				.tx_phys = BLE_GAP_PHY_AUTO,
                .rx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for handling events from the GATT library. */
void BleInterface::gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}

/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
void BleInterface::nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for handling the events from the STM OTA BLE Service.
 *
 * @details This function will process the events received from the STM OTA BLE Service
 *
 * @param[in] p_evt       STM OTA Service event.
 */
/**@snippet [Handling the data received over BLE] */
void BleInterface::stm_ota_event_handler(uint16_t conn_handle, ble_stm_ota_t * p_stm_ota, ble_stm_ota_event_t * p_evt)
{
	BLEServiceEvent s_evt;
	switch (p_evt->event_type)
	{
		case STM_OTA_EVENT_ACTION:
		{
			if (p_evt->action == STM_OTA_ACTION_START_WIRELESS ||
			    p_evt->action == STM_OTA_ACTION_START_USER_APP)
			{
				// The next receive raw data packet will contain at least 8 bytes
				// which shall include the total file size and a CRC32 checksum
				m_is_first_ota_packet = true;
			}
			else if (p_evt->action == STM_OTA_ACTION_STOP_ALL ||
					p_evt->action == STM_OTA_ACTION_CANCEL)
			{
				s_evt.event_type = BLEServiceEventType::OTA_ABORT;
				m_on_event(s_evt);
				ble_stm_ota_on_file_upload_end_status(conn_handle, p_stm_ota, STM_OTA_FILE_UPLOAD_STATUS_NOT_OK);
			}
			else if (p_evt->action == STM_OTA_ACTION_UPLOAD_FINISHED)
			{
				s_evt.event_type = BLEServiceEventType::OTA_END;
				try {
					m_on_event(s_evt);
				} catch (ErrorCode e) {
					// Exception raised, indicate the procedure failed
					ble_stm_ota_on_file_upload_end_status(conn_handle, p_stm_ota, STM_OTA_FILE_UPLOAD_STATUS_NOT_OK);
				}
			}
		}
		break;
		case STM_OTA_EVENT_RAW_DATA:
		{
			if (m_is_first_ota_packet)
			{
				// If the length is less than 8 bytes then flag an error
				if (p_evt->length < 8)
				{
					ble_stm_ota_on_file_upload_end_status(conn_handle, p_stm_ota, STM_OTA_FILE_UPLOAD_STATUS_NOT_OK);
					return;
				}

				m_is_first_ota_packet = false;

				s_evt.event_type = BLEServiceEventType::OTA_START;
				s_evt.file_size = ((uint8_t *)p_evt->p_data)[0] |
						((uint8_t *)p_evt->p_data)[1] << 8 |
						((uint8_t *)p_evt->p_data)[2] << 16 |
						((uint8_t *)p_evt->p_data)[3] << 24;
				s_evt.crc32 = ((uint8_t *)p_evt->p_data)[4] |
						((uint8_t *)p_evt->p_data)[5] << 8 |
						((uint8_t *)p_evt->p_data)[6] << 16 |
						((uint8_t *)p_evt->p_data)[7] << 24;

				// Notify start event
				try {
					m_on_event(s_evt);
				} catch (ErrorCode e) {
					// Exception raised, indicate the procedure failed
					ble_stm_ota_on_file_upload_end_status(conn_handle, p_stm_ota, STM_OTA_FILE_UPLOAD_STATUS_NOT_OK);
				}

				// If any bytes remain then pass these on as file data
				if (p_evt->length > 8)
				{
					s_evt.event_type = BLEServiceEventType::OTA_FILE_DATA;
					s_evt.data = (uint8_t *)p_evt->p_data + 8;
					s_evt.length = p_evt->length - 8;

					// Notify file data
					try {
						m_on_event(s_evt);
					} catch (ErrorCode e) {
						// Exception raised, indicate the procedure failed
						ble_stm_ota_on_file_upload_end_status(conn_handle, p_stm_ota, STM_OTA_FILE_UPLOAD_STATUS_NOT_OK);
					}
				}
			}
			else
			{
				s_evt.event_type = BLEServiceEventType::OTA_FILE_DATA;
				s_evt.data = p_evt->p_data;
				s_evt.length = p_evt->length;

				try {
					m_on_event(s_evt);
				} catch (ErrorCode e) {
					// Exception raised, indicate the procedure failed
					ble_stm_ota_on_file_upload_end_status(conn_handle, p_stm_ota, STM_OTA_FILE_UPLOAD_STATUS_NOT_OK);
				}
			}

		}
		break;
		default:
			break;
	}
}

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
void BleInterface::nus_data_handler(ble_nus_evt_t * p_evt)
{
    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        if (!m_carriage_return_received)
        {
            // If receiving this data would overrun our buffer then we need to discard what we already have to prevent the buffer from filling and not being cleared
            if (m_receive_buffer_len + p_evt->params.rx_data.length > sizeof(m_receive_buffer))
                m_receive_buffer_len = 0;

            if (p_evt->params.rx_data.length > sizeof(m_receive_buffer))
            {
                NRF_LOG_WARNING("Received data from BLE NUS larger than buffer.");
            }
            else
            {
                memcpy(m_receive_buffer, p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
                m_receive_buffer_len += p_evt->params.rx_data.length;
                if (m_receive_buffer[m_receive_buffer_len - 1] == '\r')
                    m_carriage_return_received = true;
                
                // Notify the user that we have a string waiting to be read with readline()
                if (m_on_event) {
                	BLEServiceEvent e;
                	e.event_type = BLEServiceEventType::DTE_DATA_RECEIVED;
                    (void)m_on_event(e);
                }
            }
        }
        else
        {
            NRF_LOG_WARNING("Received data from BLE NUS. But discarded as buffer already contains a string.");
        }

        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
    }

}

/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
void BleInterface::on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
			NRF_LOG_INFO("Start advertising");
            //err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            //APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            advertising_start();
            break;
		case BLE_ADV_EVT_DIRECTED_HIGH_DUTY:
		case BLE_ADV_EVT_DIRECTED:
		case BLE_ADV_EVT_SLOW:
		case BLE_ADV_EVT_FAST_WHITELIST:
		case BLE_ADV_EVT_SLOW_WHITELIST:
		case BLE_ADV_EVT_WHITELIST_REQUEST:
		case BLE_ADV_EVT_PEER_ADDR_REQUEST:
        default:
            break;
    }
}

/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
void BleInterface::on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}

/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
void BleInterface::conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}
