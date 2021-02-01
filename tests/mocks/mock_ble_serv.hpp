#ifndef __MOCK_BLE_HPP_
#define __MOCK_BLE_HPP_

#include "ble_service.hpp"
#include "mock_std_function_comparator.hpp"

class MockBLEService : public BLEService {
	void start(const std::function<void()> &on_connected, const std::function<void()> &on_disconnected, const std::function<void()> &on_received) {

		// Install a comparator for checking the equality of std::functions
		mock().installComparator("std::function<void()>", m_comparator);

		mock().actualCall("start").onObject(this).withParameterOfType("std::function<void()>", "on_connected", &on_connected).withParameterOfType("std::function<void()>", "on_disconnected", &on_disconnected).withParameterOfType("std::function<void()>", "on_received", &on_received);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void write(std::string str) {
		mock().actualCall("write").onObject(this).withParameterOfType("std::string", "str", &str);
	}
	std::string read_line() {
		return *static_cast<const std::string*>(mock().actualCall("read_line").onObject(this).returnConstPointerValue());
	}

private:
	MockStdFunctionVoidComparator m_comparator;
};

#endif // __MOCK_BLE_HPP_
