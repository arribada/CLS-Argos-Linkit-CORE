#ifndef __MOCK_ARTIC_HPP_
#define __MOCK_ARTIC_HPP_

#include "argos_scheduler.hpp"

class MockArtic : public ArgosScheduler {
public:
	std::string m_last_packet;

	void power_off() override { mock().actualCall("power_off").onObject(this); }
	void power_on() override { mock().actualCall("power_on").onObject(this); }
	void send_packet(ArgosPacket const& packet, unsigned int total_bits) override {
		m_last_packet = packet;
		mock().actualCall("send_packet").onObject(this).withParameter("total_bits", total_bits);
	}
	void set_frequency(const double freq) override {
		mock().actualCall("set_frequency").onObject(this).withParameter("freq", freq);
	}
	void set_tx_power(const BaseArgosPower power) override {
		mock().actualCall("set_tx_power").onObject(this).withParameter("power", (unsigned int)power);
	}
};

#endif // __MOCK_ARTIC_HPP_
