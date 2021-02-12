#pragma once

#include <array>
#include "gps_scheduler.hpp"
#include "nrf_uart_m8.hpp"
#include "ubx.hpp"

class M8QReceiver : public GPSScheduler {
public:
	M8QReceiver();

private:
	// These methods are specific to the chipset and should be implemented by device-specific subclass
	void power_off() override;
	void power_on(std::function<void(GNSSData data)> data_notification_callback) override;

	enum class SendReturnCode
	{
		SUCCESS,
		RESPONSE_TIMEOUT,
		NACKD
	};

	template<typename T>
	SendReturnCode send_packet_contents(UBX::MessageClass msgClass, uint8_t id, T contents, bool expect_ack = true);
	void reception_callback(uint8_t *data, size_t len);
	void populate_gnss_data_and_callback();

    SendReturnCode setup_uart_port();
    SendReturnCode setup_gnss_channel_sharing();
    SendReturnCode setup_power_management();
    SendReturnCode setup_lower_power_mode();
    SendReturnCode setup_simple_navigation_settings();
    SendReturnCode setup_expert_navigation_settings();
	SendReturnCode disable_odometer();
    SendReturnCode disable_timepulse_output();
    SendReturnCode enable_nav_pvt_message();
    SendReturnCode enable_nav_dop_message();

	NrfUARTM8 *m_nrf_uart_m8;
	bool m_has_booted;
	std::function<void(GNSSData data)> m_data_notification_callback;

	UBX::NAV::PVT::MSG_PVT m_last_received_pvt;
	UBX::NAV::DOP::MSG_DOP m_last_received_dop;

	struct {
		volatile bool pending;
		size_t len;
		std::array<uint8_t, UBX::MAX_PACKET_LEN> data;
	} m_rx_buffer;
};
