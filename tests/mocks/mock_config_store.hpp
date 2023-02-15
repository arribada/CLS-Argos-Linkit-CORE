#pragma once

#include "config_store.hpp"

class MockConfigurationStore : public ConfigurationStore {
protected:
	bool m_is_valid;
	void serialize_config() {}
	void serialize_zone() {}
	void update_battery_level() {}

public:
	BasePassPredict m_pass_predict;

	MockConfigurationStore() : ConfigurationStore() { m_is_valid = true; }
	void set_is_valid(bool state) { m_is_valid = state; }

	void init() {
		m_params = default_params;
		mock().actualCall("init").onObject(this);
	}
	bool is_valid() {
		return m_is_valid;
	}
	bool is_zone_valid() {
		return mock().actualCall("is_zone_valid").onObject(this).returnBoolValue();
	}
	void factory_reset() {
		mock().actualCall("factory_reset").onObject(this);
	}
	BasePassPredict& read_pass_predict() {
		mock().actualCall("read").onObject(this);
		return m_pass_predict;
	}
	void write_pass_predict(BasePassPredict& value) {
		mock().actualCall("write").onObject(this);
		m_pass_predict = value;
	}
	bool is_battery_level_low() {
		return mock().actualCall("is_battery_level_low").onObject(this).returnBoolValue();
	}
};
