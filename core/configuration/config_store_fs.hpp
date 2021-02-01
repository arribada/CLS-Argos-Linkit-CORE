#ifndef __CONFIG_STORE_FS_HPP_
#define __CONFIG_STORE_FS_HPP_

#include <cstring>
#include "config_store.hpp"
#include "filesystem.hpp"
#include "debug.hpp"
#include "battery.hpp"

extern BatteryMonitor *battery_monitor;

class LFSConfigurationStore : public ConfigurationStore {

protected:
	bool m_is_pass_predict_valid;
	bool m_is_zone_valid;
	bool m_is_config_valid;
	BasePassPredict m_pass_predict;

	bool serialize_config_entry(LFSFile &f, BaseType& d) {

		// std::string is dynamically allocated so we can't write the std::variant
		// directly and have to translate it first to a char array

		char entry[BASE_TEXT_MAX_LENGTH];
		if (d.index() == 0) {  // std::string type
			std::strcpy(entry, std::get<std::string>(d).c_str());
			return f.write(entry, BASE_TEXT_MAX_LENGTH) == BASE_TEXT_MAX_LENGTH;
		} else {   			// All other types
			return f.write(&d, sizeof(BaseType)) == sizeof(BaseType) &&
				f.write(entry, BASE_TEXT_MAX_LENGTH - sizeof(BaseType)) == BASE_TEXT_MAX_LENGTH - sizeof(BaseType);
		}
	}

	bool deserialize_config_entry(LFSFile &f, BaseType& d) {

		// std::string is dynamically allocated so we can't read into the std::variant
		// directly and have to translate it first from a char array

		char entry[BASE_TEXT_MAX_LENGTH];
		if (d.index() == 0) {  // std::string type
			if (f.read(entry, BASE_TEXT_MAX_LENGTH) != BASE_TEXT_MAX_LENGTH)
				return false;
			d = std::string(entry);
			return true;
		} else {             // All other types
			return f.read(&d, sizeof(BaseType)) == sizeof(BaseType) &&
					f.read(entry, BASE_TEXT_MAX_LENGTH - sizeof(BaseType));
		}
	}

	void deserialize_config() {
		DEBUG_TRACE("deserialize_config");
		LFSFile f(&m_filesystem, "config.dat", LFS_O_RDONLY);
		unsigned int i;

		for (i = 0; i < MAX_CONFIG_ITEMS; i++) {
			if (!deserialize_config_entry(f, m_params.at(i)))
				break;
		}

		m_is_config_valid = (i == MAX_CONFIG_ITEMS);
	}

	void serialize_config(ParamID param_id) override {
		DEBUG_TRACE("serialize_config(%u)", (unsigned)param_id);
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY);
		if (f.seek((signed)param_id * BASE_TEXT_MAX_LENGTH) != (signed)param_id * BASE_TEXT_MAX_LENGTH ||
			!serialize_config_entry(f, m_params.at((unsigned)param_id))) {
			m_is_config_valid = false;
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void deserialize_zone() {
		LFSFile f(&m_filesystem, "zone.dat", LFS_O_RDWR);
		if (f.read(&m_zone, sizeof(m_zone)) == sizeof(m_zone))
			m_is_zone_valid = true;
	}

	void serialize_zone() {
		DEBUG_TRACE("serialize_zone");
		LFSFile f(&m_filesystem, "zone.dat", LFS_O_CREAT | LFS_O_WRONLY);
		m_is_zone_valid = false;
		m_is_zone_valid = f.write(&m_zone, sizeof(m_zone)) == sizeof(m_zone);
		DEBUG_TRACE("m_is_zone_valid = %u", m_is_zone_valid);
		if (!m_is_zone_valid) {
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void serialize_pass_predict() {
		DEBUG_TRACE("serialize_pass_predict");
		LFSFile f(&m_filesystem, "pass_predict.dat", LFS_O_WRONLY);
		m_is_pass_predict_valid = false;
		m_is_pass_predict_valid = f.write(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict);
	}

	void create_default_config() {
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT);
		unsigned int i;
		for (i = 0; i < MAX_CONFIG_ITEMS; i++) {
			if (!serialize_config_entry(f, m_params.at(i)))
				break;
		}
		m_is_config_valid = i == MAX_CONFIG_ITEMS;

		DEBUG_TRACE("Created new config.data is_valid=%u", m_is_config_valid);
	}

	void create_default_zone() {
		write_zone((BaseZone&)default_zone);
	}

	void update_battery_level() {
		m_battery_level = battery_monitor->get_level();
		m_battery_voltage = battery_monitor->get_voltage();
	}

private:
	FileSystem &m_filesystem;

public:
	LFSConfigurationStore(FileSystem &filesystem) : m_filesystem(filesystem) {}

	void init() {
		m_is_zone_valid = false;
		m_is_pass_predict_valid = false;
		m_is_config_valid = false;

		// Copy default params so we have an initial working set
		m_params = default_params;

		// Read in configuration file or create new one if it doesn't not exist
		try {
			deserialize_config();
		} catch (int e) {
			DEBUG_WARN("No configuration file so creating a default file");
			create_default_config();
		}

		if (!m_is_config_valid)
			throw CONFIG_STORE_CORRUPTED; // This is a non-recoverable error

		// Read in zone file
		try {
			deserialize_zone();
		} catch (int e) {
			DEBUG_WARN("Zone file does not exist");
			create_default_zone();
		}

		// Read in pass predict file
		m_is_pass_predict_valid = false;
		try {
			LFSFile f(&m_filesystem, "pass_predict.dat", LFS_O_CREAT | LFS_O_RDWR);
			if (f.read(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict))
				m_is_pass_predict_valid = true;
		} catch (int e) {
			DEBUG_WARN("Prepass file does not exist");
		}
	}

	bool is_valid() override {
		return m_is_config_valid;
	}

	bool is_zone_valid() override {
		return m_is_zone_valid;
	}

	void notify_saltwater_switch_state(bool state) override {
		(void)state;
	}

	void factory_reset() override {
		m_filesystem.format();
	}

	BasePassPredict& read_pass_predict() override {
		if (m_is_pass_predict_valid) {
			return m_pass_predict;
		}
		throw CONFIG_DOES_NOT_EXIST;
	}

	void write_pass_predict(BasePassPredict& value) override {
		m_pass_predict = value;
		serialize_pass_predict();
	}

	bool is_battery_level_low() {
		auto lb_en = read_param<bool>(ParamID::LB_EN);
		auto lb_threshold = read_param<unsigned int>(ParamID::LB_TRESHOLD);
		update_battery_level();
		return (lb_en && m_battery_level <= lb_threshold);
	}

};

#endif // __CONFIG_STORE_FS_HPP_
