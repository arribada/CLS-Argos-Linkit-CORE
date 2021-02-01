#ifndef __MOCK_BATTERY_MON_HPP_
#define __MOCK_BATTERY_MON_HPP_

#include "battery.hpp"

class MockBatteryMonitor : public BatteryMonitor {
public:
	void start() override {
		mock().actualCall("start").onObject(this);
	}
	void stop() override {
		mock().actualCall("stop").onObject(this);
	}
	uint16_t get_voltage() override {
		return mock().actualCall("get_voltage").onObject(this).returnUnsignedIntValue();
	}
	uint8_t get_level() override {
		return mock().actualCall("get_level").onObject(this).returnUnsignedIntValue();
	}
};

#endif // __MOCK_ARTIC_HPP_
