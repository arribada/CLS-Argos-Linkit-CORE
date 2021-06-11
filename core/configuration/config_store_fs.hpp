#ifndef __CONFIG_STORE_FS_HPP_
#define __CONFIG_STORE_FS_HPP_

#include <cstring>
#include "config_store.hpp"
#include "filesystem.hpp"
#include "debug.hpp"
#include "battery.hpp"
#include "dte_params.hpp"


extern BatteryMonitor *battery_monitor;

class LFSConfigurationStore : public ConfigurationStore {

protected:
	bool m_is_pass_predict_valid;
	bool m_is_zone_valid;
	bool m_is_config_valid;
	BasePassPredict m_pass_predict;

	// Serialization format is as follows:
	//
	// <KEY><BINARY_DATA_ZERO_PADDED_TO_128_BYTES>
	//
	// Note that <KEY> must match the entry at index inside the
	// param_map table otherwise the configuration item is
	// rejected.  <KEY> has fixed length of 5 e.g., "IDT01".

	bool serialize_config_entry(LFSFile &f, unsigned int index) {
		if (index >= MAX_CONFIG_ITEMS)
			return false;
		const BaseMap* entry = &param_map[index];
		struct Serializer {
			uint8_t entry_buffer[BASE_TEXT_MAX_LENGTH];
			void operator()(std::string& s) {
				std::memset(entry_buffer, 0, sizeof(entry_buffer));
				std::memcpy(entry_buffer, s.data(), s.size());
			};
			// BaseGNSSDynModel
			void operator()(unsigned int &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(int &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(double &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(std::time_t &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosPower &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseArgosDepthPile &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(bool &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseGNSSFixMode &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseGNSSDynModel &s) {
				std::memcpy(entry_buffer, &s, sizeof(s));
			};
			void operator()(BaseRawData &) {
			};
		} s;
		std::visit(s, m_params.at(index));

		return f.write((void *)entry->key.data(), entry->key.size()) == (lfs_ssize_t)entry->key.size() &&
				f.write(s.entry_buffer, sizeof(s.entry_buffer)) == sizeof(s.entry_buffer);
	}

	bool deserialize_config_entry(LFSFile &f, unsigned int index) {
		if (index >= MAX_CONFIG_ITEMS)
			return false;
		const BaseMap* entry = &param_map[index];
		uint8_t entry_buffer[KEY_LENGTH + BASE_TEXT_MAX_LENGTH];

		// Read entry from file
		if (f.read(entry_buffer, KEY_LENGTH + BASE_TEXT_MAX_LENGTH) != KEY_LENGTH + BASE_TEXT_MAX_LENGTH)
			return false;

		// Check param key matches
		if (std::string((const char *)entry_buffer, KEY_LENGTH) != entry->key)
			return false;

		uint8_t *param_value = &entry_buffer[KEY_LENGTH];

		switch (entry->encoding) {
		case BaseEncoding::DECIMAL:
		{
			int value = *(int *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::AQPERIOD:
		case BaseEncoding::HEXADECIMAL:
		case BaseEncoding::UINT:
		{
			unsigned int value = *(unsigned int *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::TEXT:
		{
			std::string value((const char *)param_value);
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::DATESTRING:
		{
			std::time_t value = *(std::time_t *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::BOOLEAN:
		{
			bool value = *(bool *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ARGOSFREQ:
		case BaseEncoding::FLOAT:
		{
			double value = *(double *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::DEPTHPILE:
		{
			BaseArgosDepthPile value = *(BaseArgosDepthPile *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ARGOSMODE:
		{
			BaseArgosMode value = *(BaseArgosMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::ARGOSPOWER:
		{
			BaseArgosPower value = *(BaseArgosPower *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::GNSSFIXMODE:
		{
			BaseGNSSFixMode value = *(BaseGNSSFixMode *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::GNSSDYNMODEL:
		{
			BaseGNSSDynModel value = *(BaseGNSSDynModel *)param_value;
			m_params.at(index) = value;
			break;
		}
		case BaseEncoding::KEY_LIST:
		case BaseEncoding::KEY_VALUE_LIST:
		case BaseEncoding::BASE64:
		default:
			return false;
		}

		return true;
	}

	void deserialize_config() {
		DEBUG_TRACE("ConfigurationStoreLFS::deserialize_config");
		LFSFile f(&m_filesystem, "config.dat", LFS_O_RDONLY);

		unsigned int config_version_code = 0;

		// Read configuration version field
		if (f.read(&config_version_code, sizeof(config_version_code)) != sizeof(config_version_code) ||
				config_version_code != m_config_version_code) {

			DEBUG_WARN("deserialize_config: configuration version mismatch; try to recover protected & reset all others");

			// Try to recover the protected fields
			if (!deserialize_config_entry(f, (unsigned int)ParamID::ARGOS_DECID))
				DEBUG_WARN("deserialize_config: failed to recover ARGOS_DECID");
			else
				DEBUG_INFO("deserialize_config: recovered ARGOS_DECID");
			if (!deserialize_config_entry(f, (unsigned int)ParamID::ARGOS_HEXID))
				DEBUG_WARN("deserialize_config: failed to recover ARGOS_HEXID");
			else
				DEBUG_INFO("deserialize_config: recovered ARGOS_HEXID");

			m_requires_serialization = true;
			return;
		}

		for (unsigned int i = 0; i < MAX_CONFIG_ITEMS; i++) {

			if (!deserialize_config_entry(f, i)) {
				DEBUG_WARN("deserialize_config: unable to deserialize param %s - resetting...", param_map[i].name.c_str());
				// Reset parameter to factory default
				m_params.at(i) = default_params.at(i);
				m_requires_serialization = true;
				continue;
			}

			// Check variant index (type) matches default parameters
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_WARN("deserialize_config: param %s variant index mismatch expected %u but got %u - resetting...",
						param_map[i].name.c_str(),
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to factory default
				m_params.at(i) = default_params.at(i);
				m_requires_serialization = true;
				continue;
			}
		}

		m_is_config_valid = true;
	}

	void serialize_config() override {
		DEBUG_TRACE("ConfigurationStoreLFS::serialize_config");
		LFSFile f(&m_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);

		// Write configuration version field
		if (f.write((void *)&m_config_version_code, sizeof(m_config_version_code)) != sizeof(m_config_version_code))
			throw CONFIG_STORE_CORRUPTED;

		for (unsigned int i = 0; i < MAX_CONFIG_ITEMS; i++) {

			// Check variant index (type) matches default parameter
			if (m_params.at(i).index() != default_params.at(i).index()) {
				DEBUG_WARN("serialize_config: param %s variant index mismatch expected %u but got %u - resetting...",
						param_map[i].name.c_str(),
						default_params.at(i).index(),
						m_params.at(i).index());
				// Reset parameter to back to factory default
				m_params.at(i) = default_params.at(i);
			}

			if (!serialize_config_entry(f, i)) {
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

		// Write configuration version field
		if (f.write((void *)&m_config_version_code, sizeof(m_config_version_code)) != sizeof(m_config_version_code))
			throw CONFIG_STORE_CORRUPTED;

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

			if (!serialize_config_entry(f, i)) {
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
