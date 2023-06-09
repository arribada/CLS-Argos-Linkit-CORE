#pragma once

#include <cstdint>
#include "events.hpp"

struct BatteryMonitorEventVoltageCritical {};

class BatteryMonitorEventListener {
public:
	virtual ~BatteryMonitorEventListener() {}
	virtual void react(BatteryMonitorEventVoltageCritical const &) {};
};

class BatteryMonitor : public EventEmitter<BatteryMonitorEventListener> {
protected:
	uint16_t m_last_voltage_mv;
	uint8_t  m_last_level;
	uint16_t m_critical_voltage_mv;
	uint8_t  m_low_level;
	bool     m_is_low_level;
	bool     m_is_critical_voltage;

private:
	bool     m_is_critical_voltage_last;

	void actuate_events() {
		if (m_is_critical_voltage && !m_is_critical_voltage_last) {
			notify<BatteryMonitorEventVoltageCritical>({});
		}
		m_is_critical_voltage_last = m_is_critical_voltage;
	}
	virtual void internal_update() {}

public:
	BatteryMonitor(uint8_t low_level, uint16_t critical_voltage) :
		m_last_voltage_mv(0), m_last_level(0),
		m_critical_voltage_mv(critical_voltage), m_low_level(low_level),
		m_is_low_level(false),
		m_is_critical_voltage(false),
		m_is_critical_voltage_last(false) {}
	virtual ~BatteryMonitor() {}
	uint16_t get_voltage() { return m_last_voltage_mv; }
	uint8_t get_level() { return m_last_level; }
	bool is_battery_low() { return m_is_low_level; }
	bool is_battery_critical() { return m_is_critical_voltage; }
	void update() {
		internal_update();
		actuate_events();
	}
};
