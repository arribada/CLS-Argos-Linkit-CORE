#ifndef __CONFIG_STORE_HPP_
#define __CONFIG_STORE_HPP_

#include "error.hpp"
#include "base_types.hpp"

#define MAX_CONFIG_PARAMS  50

class ConfigurationStore {

protected:
	BaseType m_params[MAX_CONFIG_PARAMS];
	virtual void serialize_config(ParamID) = 0;

public:
	virtual void init() = 0;
	virtual bool is_valid() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
	virtual void factory_reset() = 0;
	virtual BaseZone& read(uint8_t zone_id=1) = 0;
	virtual BasePassPredict& read() = 0;
	virtual void write(BaseZone& value) = 0;
	virtual void write(BasePassPredict& value) = 0;

	template <typename T>
	T& read(ParamID param_id) {
		if (is_valid())
			return std::get<T>(m_params[(unsigned)param_id]);
		else
			throw CONFIG_STORE_CORRUPTED;
	}

	template<typename T>
	void write(ParamID param_id, T& value) {
		m_params[(unsigned)param_id] = value;
		serialize_config(param_id);
	}
};

#endif // __CONFIG_STORE_HPP_
