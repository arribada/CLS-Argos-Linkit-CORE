#ifndef __MOCK_BLE_HPP_
#define __MOCK_BLE_HPP_

#include "ble_service.hpp"

class MockBLEService : public BLEService {
	void start(void (*on_connected)(void), void (*on_disconnected)(void)) {
		mock().actualCall("start").onObject(this).withParameter("on_connected", on_connected).withParameter("on_disconnected", on_disconnected);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
};

#endif // __MOCK_BLE_HPP_
