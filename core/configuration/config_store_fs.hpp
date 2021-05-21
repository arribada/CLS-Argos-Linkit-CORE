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
		DEBUG_TRACE("ConfigurationStoreLFS::deserialize_config");
		LFSFile f(&m_filesystem, "config.dat", LFS_O_RDONLY);

		for (unsigned int i = 0; i < MAX_CONFIG_ITEMS; i++) {

			if (!deserialize_config_entry(f, m_params.at(i))) {
				DEBUG_WARN("deserialize_config: unable to deserialize param %u - resetting...", i);
				// Reset parameter to factory default
				m_params.at(i) = default_params.at(i);
				m_requires_serialization = true;
			}

			// Check variant index (type) matches default parameters
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_WARN("deserialize_config: param %u variant index mismatch expected %u but got %u - resetting...",
						i,
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to factory default
				m_params.at(i) = default_params.at(i);
				m_requires_serialization = true;
			}
		}

		m_is_config_valid = true;
	}

	void serialize_config() override {
		DEBUG_TRACE("ConfigurationStoreLFS::serialize_config");
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

		for (unsigned int i = 0; i < MAX_CONFIG_ITEMS; i++) {

			// Check variant index (type) matches default parameter
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_WARN("serialize_config: param %u variant index mismatch expected %u but got %u - resetting...",
						i,
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to back to factory default
				m_params.at(i) = default_params.at(i);
			}

			if (!serialize_config_entry(f, m_params.at(i))) {
				DEBUG_ERROR("serialize_config: failed to serialize param %u", i);
				throw CONFIG_STORE_CORRUPTED;
			}
		}

		m_is_config_valid = true;

		DEBUG_INFO("ConfigurationStoreLFS::serialize_config: saved new file config.data");
	}

	void deserialize_zone() {
		DEBUG_TRACE("ConfigurationStoreLFS::deserialize_zone");
		LFSFile f(&m_filesystem, "zone.dat", LFS_O_RDWR);
		if (f.read(&m_zone, sizeof(m_zone)) == sizeof(m_zone))
			m_is_zone_valid = true;
	}

	void serialize_zone() override {
		DEBUG_TRACE("ConfigurationStoreLFS::serialize_zone");
		LFSFile f(&m_filesystem, "zone.dat", LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC);
		m_is_zone_valid = false;
		m_is_zone_valid = f.write(&m_zone, sizeof(m_zone)) == sizeof(m_zone);
		if (!m_is_zone_valid) {
			DEBUG_ERROR("serialize_zone: failed to serialize zone");
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void serialize_pass_predict() {
		DEBUG_TRACE("ConfigurationStoreLFS::serialize_pass_predict");
		LFSFile f(&m_filesystem, "pass_predict.dat", LFS_O_CREAT | LFS_O_WRONLY | LFS_O_TRUNC);
		m_is_pass_predict_valid = false;
		m_is_pass_predict_valid = f.write(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict);
		if (!m_is_pass_predict_valid) {
			DEBUG_ERROR("serialize_pass_predict: failed to serialize pass predict");
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void create_default_zone() {
		DEBUG_TRACE("ConfigurationStoreLFS::create_default_zone");
		write_zone((BaseZone&)default_zone);
	}

	void update_battery_level() override {
		m_battery_level = battery_monitor->get_level();
		m_battery_voltage = battery_monitor->get_voltage();
	}

private:
	FileSystem &m_filesystem;
	bool        m_requires_serialization;

	void serialize_protected_config() {
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
		for (unsigned int i = 0; i <= (unsigned int)ParamID::ARGOS_HEXID ; i++) {
			// Check variant index (type) matches default parameter
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_TRACE("serialize_config: protected param %u variant index mismatch expected %u but got %u - repairing",
						i,
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to back to factory default
				m_params.at(i) = default_params.at(i);
			}

			if (!serialize_config_entry(f, m_params.at(i))) {
				DEBUG_TRACE("serialize_config: failed to serialize protected param %u", i);
				throw CONFIG_STORE_CORRUPTED;
			}
		}

		DEBUG_TRACE("ConfigurationStoreLFS::serialize_protected_config: saved protected params to config.data");
	}

public:
	LFSConfigurationStore(FileSystem &filesystem) : m_is_pass_predict_valid(false), m_is_zone_valid(false), m_is_config_valid(false), m_filesystem(filesystem) {}

	void init() override {
		m_requires_serialization = false;
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
			m_requires_serialization = true;
		}

		if (m_requires_serialization)
			serialize_config();

		if (!m_is_config_valid)
			throw CONFIG_STORE_CORRUPTED; // This is a non-recoverable error

		// Read in zone file
		try {
			deserialize_zone();
		} catch (int e) {
			DEBUG_WARN("Zone file does not exist or is corrupted - resetting zone file");
			create_default_zone();
		}

		// Read in pass predict file
		m_is_pass_predict_valid = false;
		try {
			LFSFile f(&m_filesystem, "pass_predict.dat", LFS_O_RDWR);
			if (f.read(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict))
				m_is_pass_predict_valid = true;
		} catch (int e) {
			DEBUG_WARN("Prepass file does not exist - requires configuration");
		}
	}

	bool is_valid() override {
		return m_is_config_valid;
	}

	bool is_zone_valid() override {
		return m_is_zone_valid;
	}

	void factory_reset() override {
		m_filesystem.umount();
		m_filesystem.format();
		m_filesystem.mount();
		serialize_protected_config(); // Recover "protected" parameters
		m_is_config_valid = false;
		m_is_pass_predict_valid = false;
		m_is_zone_valid = false;
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

	bool is_battery_level_low() override {
		auto lb_en = read_param<bool>(ParamID::LB_EN);
		auto lb_threshold = read_param<unsigned int>(ParamID::LB_TRESHOLD);
		update_battery_level();
		return (lb_en && m_battery_level <= lb_threshold);
	}

};

#endif // __CONFIG_STORE_FS_HPP_
