#pragma once

#include "battery.hpp"

class FakeBatteryMonitor : public BatteryMonitor {
public:
	FakeBatteryMonitor() : BatteryMonitor(10, 2200) {}
	void internal_update() {}
	void set_values(uint8_t level = 100, uint16_t mv = 4200, bool is_low = false, bool is_critical = false) {
		m_last_voltage_mv = mv;
		m_last_level = level;
		m_is_critical_voltage = is_critical;
		m_is_low_level = is_low;
	}
};
