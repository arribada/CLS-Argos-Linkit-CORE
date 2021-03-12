#pragma once

#include <array>
#include "gps_scheduler.hpp"
#include "nrf_uart_m8.hpp"
#include "ubx.hpp"

class M8QReceiver : public GPSScheduler {
public:
	M8QReceiver();
	~M8QReceiver();

private:
	// These methods are specific to the chipset and should be implemented by a device-specific subclass
	void power_off() override;
	void power_on(std::function<void(GNSSData data)> data_notification_callback) override;

	enum class SendReturnCode
	{
		SUCCESS,
		RESPONSE_TIMEOUT,
		MISSING_DATA,
		DATA_OVERSIZE,
		NACKD
	};

	enum class State
	{
		POWERED_OFF,
		POWERED_ON
	} m_state;

	uint8_t m_navigation_database[16384];
	uint32_t m_navigation_database_len;

	template<typename T>
	SendReturnCode send_packet_contents(UBX::MessageClass msgClass, uint8_t id, T contents, bool expect_ack = true);

	template<typename T>
	SendReturnCode poll_contents_and_collect(UBX::MessageClass msgClass, uint8_t id, T &contents);

	SendReturnCode poll_contents(UBX::MessageClass msgClass, uint8_t id);

	void reception_callback(uint8_t *data, size_t len);
	void populate_gnss_data_and_callback();

    SendReturnCode setup_uart_port();
    SendReturnCode setup_gnss_channel_sharing();
    SendReturnCode setup_power_management();
    SendReturnCode setup_lower_power_mode();
    SendReturnCode setup_simple_navigation_settings();
    SendReturnCode setup_expert_navigation_settings();
	SendReturnCode supply_time_assistance();
	SendReturnCode disable_odometer();
    SendReturnCode disable_timepulse_output();
    SendReturnCode enable_nav_pvt_message();
    SendReturnCode enable_nav_dop_message();
    SendReturnCode disable_nav_pvt_message();
    SendReturnCode disable_nav_dop_message();

	SendReturnCode print_version();

	SendReturnCode fetch_navigation_database();
	SendReturnCode send_navigation_database();

	NrfUARTM8 *m_nrf_uart_m8;
	bool m_capture_messages;
	std::function<void(GNSSData data)> m_data_notification_callback;

	UBX::NAV::PVT::MSG_PVT m_last_received_pvt;
	UBX::NAV::DOP::MSG_DOP m_last_received_dop;

	struct {
		volatile bool pending;
		size_t len;
		std::array<uint8_t, UBX::MAX_PACKET_LEN> data;
	} m_rx_buffer;
};
