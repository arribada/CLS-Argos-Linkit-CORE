#include "artic.hpp"
#include "artic_firmware.hpp"

void ArticTransceiver::power_off() {
}

void ArticTransceiver::power_on() {
}

void ArticTransceiver::send_packet(ArgosPacket const& packet, unsigned int total_bits) {
	(void)packet;
	(void)total_bits;
}

void ArticTransceiver::set_frequency(const double freq) {
	(void)freq;
}

void ArticTransceiver::set_tx_power(const BaseArgosPower power) {
	(void)power;
}
