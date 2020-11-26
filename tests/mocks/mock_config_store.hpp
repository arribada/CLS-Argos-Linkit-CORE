#ifndef __MOCK_CONFIG_HPP_
#define __MOCK_CONFIG_HPP_

#include "config_store.hpp"

class MockConfigurationStore : public ConfigurationStore {
public:
	void init() {
		mock().actualCall("init").onObject(this);
	}
	bool is_valid() {
		return mock().actualCall("is_valid").onObject(this).returnBoolValue();
	}
	void notify_saltwater_switch_state(bool state) {
		DEBUG_TRACE("MockConfigurationStore: notify_saltwater_switch_state");
		mock().actualCall("notify_saltwater_switch_state").onObject(this).withParameter("state", state);
	}
};

#endif // __MOCK_CONFIG_HPP_
