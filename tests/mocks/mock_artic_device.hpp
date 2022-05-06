#pragma once

#include "artic_device.hpp"
#include "binascii.hpp"

class MockArticDevice : public ArticDevice {
public:
	void send(const ArticMode mode, ArticPacket const&, unsigned int size_bits) override {
		mock().actualCall("send").onObject(this).withParameter("mode", (unsigned int)mode).withParameter("size_bits", size_bits);
	}
	void send_ack(const ArticMode mode, const unsigned int a_dcs, const unsigned int dl_msg_id, const unsigned int exec_report) override {
		mock().actualCall("send_ack").onObject(this).withParameter("mode", (unsigned int)mode).
				withParameter("a_dcs", a_dcs).
				withParameter("dl_msg_id", dl_msg_id).
				withParameter("exec_report", exec_report);
	}
	void stop_send() override {
		mock().actualCall("stop_send").onObject(this);
	}
	void start_receive(const ArticMode mode) override {
		mock().actualCall("start_receive").onObject(this).withParameter("mode", (unsigned int)mode);
	}
	void stop_receive() override {
		mock().actualCall("stop_receive").onObject(this);
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
	unsigned int get_cumulative_receive_time() override {
		return mock().actualCall("get_cumulative_receive_time").onObject(this).returnUnsignedIntValue();
	}
	void inject_rx_packet(std::string packet, unsigned int sz) {
		ArticEventRxPacket e;
		e.packet = Binascii::unhexlify(packet);
		e.size_bits = sz;
		notify(e);
	}
};
