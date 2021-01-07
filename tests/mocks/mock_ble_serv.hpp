#ifndef __MOCK_BLE_HPP_
#define __MOCK_BLE_HPP_

#include "ble_service.hpp"

class MockBLEService : public BLEService {
	void start(const std::function<void ()> & on_connected, const std::function<void ()> & on_disconnected, const std::function<void ()> & on_received) {
		mock().actualCall("BLEService::start").onObject(this).withParameterOfType("()>", "on_connected", &on_connected).withParameterOfType("()>", "on_disconnected", &on_disconnected).withParameterOfType("()>", "on_received", &on_received);
	}
	void stop() {
		mock().actualCall("BLEService::stop").onObject(this);
	}
	void write(std::string str) {
		mock().actualCall("BLEService::write").onObject(this).withParameterOfType("std::string", "str", &str);
	}
	std::string read_line() {
		return *static_cast<const std::string*>(mock().actualCall("BLEService::read_line").onObject(this).returnConstPointerValue());
	}
};

#endif // __MOCK_BLE_HPP_
