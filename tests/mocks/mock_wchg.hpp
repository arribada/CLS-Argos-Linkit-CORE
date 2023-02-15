#pragma once

#include "wchg.hpp"

class MockWirelessCharger : public WirelessCharger {
public:
	std::string get_chip_status() override {
		return mock().actualCall("get_chip_status").onObject(this).returnStringValue();
	}
};
