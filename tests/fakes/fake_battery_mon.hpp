#ifndef __FAKE_BATTERY_MON_HPP_
#define __FAKE_BATTERY_MON_HPP_

#include "battery.hpp"

class FakeBatteryMonitor : public BatteryMonitor {
public:
	uint16_t m_voltage;
	uint8_t m_level;
	void start() override {};
	void stop() override {};
	uint16_t get_voltage() { return m_voltage; }
	uint8_t get_level() { return m_level; }
};

#endif // __FAKE_BATTERY_MON_HPP_
