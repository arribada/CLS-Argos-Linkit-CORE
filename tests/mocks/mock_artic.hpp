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

	MockArtic() {
		m_is_powered_on = false;
		m_is_rx_enabled = false;
	}

	void power_off() override {
		m_is_powered_on = false;
		m_is_rx_enabled = false;
		mock().actualCall("power_off").onObject(this);
		m_notification_callback(ArgosAsyncEvent::OFF);
	}
	void power_off_immediate() override {
		m_is_powered_on = false;
		m_is_rx_enabled = false;
		mock().actualCall("power_off_immediate").onObject(this);
	}
	void power_on(const unsigned int argos_id, std::function<void(ArgosAsyncEvent)> notification_callback) override {
		m_is_powered_on = true;
		m_is_rx_enabled = false;
		mock().actualCall("power_on").onObject(this).withParameter("argos_id", (unsigned int)argos_id);
		m_notification_callback = notification_callback;
		m_notification_callback(ArgosAsyncEvent::ON);
	}
	void send_packet(ArgosPacket const& user_payload, unsigned int payload_bits, const ArgosMode mode) override {
		m_last_packet = user_payload;
		mock().actualCall("send_packet").onObject(this).withParameter("payload_bits", payload_bits).withParameter("mode", (unsigned int)mode);
		m_notification_callback(ArgosAsyncEvent::TX_DONE);
	}
	void set_frequency(const double freq) override {
		mock().actualCall("set_frequency").onObject(this).withParameter("freq", freq);
	}
	void set_tcxo_warmup_time(const unsigned int time) override {
		mock().actualCall("set_tcxo_warmup_time").onObject(this).withParameter("time", time);
	}
	void set_tx_power(const BaseArgosPower power) override {
		mock().actualCall("set_tx_power").onObject(this).withParameter("power", (unsigned int)power);
	}
	void set_rx_mode(const ArgosMode, std::time_t stop_time) override {
		m_is_rx_enabled = true;
		mock().actualCall("set_rx_mode").onObject(this).withParameter("stop_time", (unsigned long)stop_time);
	}
	void inject_rx_packet(std::string packet, unsigned int length) {
		m_rx_packet = Binascii::unhexlify(packet);
		m_rx_packet_length = length;
		m_notification_callback(ArgosAsyncEvent::RX_PACKET);
	}
	uint64_t get_rx_time_on() override {
		return mock().actualCall("get_rx_time_on").onObject(this).returnUnsignedLongIntValue();
	}
	void read_packet(ArgosPacket& packet, unsigned int& size) override {
		packet = m_rx_packet;
		size = m_rx_packet_length;
	}
	void send_ack(const unsigned int a_dcs, const unsigned int dl_msg_id, const unsigned int exec_report, const ArgosMode mode) override {
		mock().actualCall("send_ack").onObject(this)
			.withParameter("a_dcs", (unsigned int)a_dcs)
			.withParameter("dl_msg_id", (unsigned int)dl_msg_id)
			.withParameter("exec_report", (unsigned int)exec_report)
			.withParameter("mode", (unsigned int)mode);
	}

	void inject_rx_timeout() {
		m_notification_callback(ArgosAsyncEvent::RX_TIMEOUT);
	}
};
