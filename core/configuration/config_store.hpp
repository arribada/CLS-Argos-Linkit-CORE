#ifndef __CONFIG_STORE_HPP_
#define __CONFIG_STORE_HPP_

class ConfigurationStore {
public:
	static void init();
	static bool is_valid();
	static void notify_saltwater_switch_state(bool);
};

#endif // __CONFIG_STORE_HPP_
