#ifndef __CONFIG_STORE_HPP_
#define __CONFIG_STORE_HPP_

class ConfigurationStore {
public:
	virtual void init() = 0;
	virtual bool is_valid() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
};

#endif // __CONFIG_STORE_HPP_
