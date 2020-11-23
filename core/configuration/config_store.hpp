#ifndef __CONFIG_STORE_HPP_
#define __CONFIG_STORE_HPP_

class ConfigurationStore {
private:
	FileSystem *m_filesystem;
public:
	ConfigurationStore(FileSystem *fs);
	bool is_valid();
	void notify_saltwater_switch_state(bool);
};

#endif // __CONFIG_STORE_HPP_
