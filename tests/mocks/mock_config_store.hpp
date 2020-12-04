#ifndef __MOCK_CONFIG_HPP_
#define __MOCK_CONFIG_HPP_

#include "config_store.hpp"

class MockConfigurationStore : public ConfigurationStore {
protected:
	void serialize_config(ParamID) {}

public:
	BaseZone m_zone;
	BasePassPredict m_pass_predict;

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
	void factory_reset() {
		mock().actualCall("factory_reset").onObject(this);
	}
	BaseZone& read_zone(uint8_t zone_id=1) {
		mock().actualCall("read").onObject(this).withParameter("zone_id", zone_id);
		return m_zone;
	}
	BasePassPredict& read_pass_predict() {
		mock().actualCall("read").onObject(this);
		return m_pass_predict;
	}
	void write_zone(BaseZone& value) {
		mock().actualCall("write").onObject(this);
		m_zone = value;
	}
	void write_pass_predict(BasePassPredict& value) {
		mock().actualCall("write").onObject(this);
		m_pass_predict = value;
	}
};

#endif // __MOCK_CONFIG_HPP_
