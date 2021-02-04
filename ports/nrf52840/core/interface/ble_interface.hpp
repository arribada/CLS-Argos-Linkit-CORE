#pragma once

#include "ble_service.hpp"
#include "ble.h"
#include "nrf_ble_gatt.h"
#include "ble_nus.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"

class BleInterface : public BLEService
{
public:
    void init();

	void start(std::function<int(BLEServiceEvent& event)> on_event) override;
	void stop() override;
    void write(std::string str) override;
    std::string read_line() override;

    static BleInterface& get_instance()
    {
        static BleInterface instance;
        return instance;
    }

private:
    // Prevent copies to enforce this as a singleton
    BleInterface() {}
    BleInterface(BleInterface const&)    = delete;
    void operator=(BleInterface const&)  = delete;

    void timers_init();
    void ble_stack_init();
    void gap_params_init();
    void gatt_init();
    void services_init();
    void advertising_init();
    void conn_params_init();

    void advertising_start();
    void advertising_stop();
    
    // Callbacks
    void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt);
    void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);
    void nrf_qwr_error_handler(uint32_t nrf_error);
    void nus_data_handler(ble_nus_evt_t * p_evt);
    void on_adv_evt(ble_adv_evt_t ble_adv_evt);
    void on_conn_params_evt(ble_conn_params_evt_t * p_evt);
    void conn_params_error_handler(uint32_t nrf_error);
    
    // Static callback functions for redirecting to this class from the Nordic BLE stack
    static void static_gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt) { get_instance().gatt_evt_handler(p_gatt, p_evt); };
    static void static_ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context) { get_instance().ble_evt_handler(p_ble_evt, p_context); };
    static void static_nrf_qwr_error_handler(uint32_t nrf_error) { get_instance().nrf_qwr_error_handler(nrf_error); };
    static void static_nus_data_handler(ble_nus_evt_t * p_evt) { get_instance().nus_data_handler(p_evt); };
    static void static_on_adv_evt(ble_adv_evt_t ble_adv_evt) { get_instance().on_adv_evt(ble_adv_evt); }
    static void static_on_conn_params_evt(ble_conn_params_evt_t * p_evt) { get_instance().on_conn_params_evt(p_evt); };
    static void static_conn_params_error_handler(uint32_t nrf_error) { get_instance().conn_params_error_handler(nrf_error); }

    uint8_t m_receive_buffer[1024];
    volatile size_t m_receive_buffer_len;
    volatile bool m_carriage_return_received;
    std::function<int(BLEServiceEvent& event)> m_on_event;
};
