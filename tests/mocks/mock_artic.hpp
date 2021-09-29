#pragma once

#include "argos_scheduler.hpp"
#include "binascii.hpp"

class MockArtic : public ArgosScheduler {
private:
	std::function<void(ArgosAsyncEvent)> m_notification_callback;
	static inline bool m_is_powered_on = false;
	static inline bool m_is_rx_enabled = false;
	ArgosPacket m_rx_packet;
	static inline unsigned int m_rx_packet_length = 0;

public:
	std::string m_last_packet;

	void power_off() override {
		m_is_powered_on = false;
		m_is_rx_enabled = false;
		mock().actualCall("power_off").onObject(this);
	}
	void power_on(std::function<void(ArgosAsyncEvent)> notification_callback) override {
		m_is_powered_on = true;
		m_is_rx_enabled = false;
		mock().actualCall("power_on").onObject(this);
		m_notification_callback = notification_callback;
		m_notification_callback(ArgosAsyncEvent::DEVICE_READY);
	}
	void send_packet(ArgosPacket const& packet, unsigned int total_bits, const ArgosMode mode) override {
		m_last_packet = packet;
		mock().actualCall("send_packet").onObject(this).withParameter("total_bits", total_bits).withParameter("mode", (unsigned int)mode);
		m_notification_callback(ArgosAsyncEvent::TX_DONE);
	}
	void set_frequency(const double freq) override {
		mock().actualCall("set_frequency").onObject(this).withParameter("freq", freq);
	}
	void set_tx_power(const BaseArgosPower power) override {
		mock().actualCall("set_tx_power").onObject(this).withParameter("power", (unsigned int)power);
	}
	void set_idle() override {
		mock().actualCall("set_idle").onObject(this);
	}
	bool is_powered_on() override {
		return m_is_powered_on;
	}
	bool is_rx_enabled() override {
		return m_is_rx_enabled;
	}
	void set_rx_mode(const ArgosMode, unsigned int) override {
		m_is_rx_enabled = true;
		mock().actualCall("set_rx_mode").onObject(this);
	}
	void inject_rx_packet(std::string packet, unsigned int length) {
		m_rx_packet = Binascii::unhexlify(packet);
		m_rx_packet_length = length;
		m_notification_callback(ArgosAsyncEvent::RX_PACKET);
	}
	void read_packet(ArgosPacket& packet, unsigned int& size) override {
		packet = m_rx_packet;
		size = m_rx_packet_length;
	}
	void inject_rx_timeout() {
		m_notification_callback(ArgosAsyncEvent::RX_TIMEOUT);
	}
};
