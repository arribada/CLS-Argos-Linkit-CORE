#ifndef __ARTIC_HPP_
#define __ARTIC_HPP_

#include "argos_scheduler.hpp"

class ArticTransceiver : public ArgosScheduler {
public:
	void power_off() override;
	void power_on() override;
	void send_packet(ArgosPacket const& packet, unsigned int total_bits) override;
	void set_frequency(const double freq) override;
	void set_tx_power(const BaseArgosPower power) override;
};

#endif // __ARTIC_HPP_
