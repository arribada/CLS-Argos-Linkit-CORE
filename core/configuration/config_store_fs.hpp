#ifndef __CONFIG_STORE_FS_HPP_
#define __CONFIG_STORE_FS_HPP_

#include <cstring>
#include "config_store.hpp"
#include "filesystem.hpp"
#include "debug.hpp"

extern FileSystem *main_filesystem;


class LFSConfigurationStore : public ConfigurationStore {

protected:
	bool m_is_pass_predict_valid;
	bool m_is_zone_valid;
	bool m_is_config_valid;
	BaseZone m_zone;
	BasePassPredict m_pass_predict;

	bool serialize_config_entry(LFSFile *f, BaseType& d) {

		// std::string is dynamically allocated so we can't write the std::variant
		// directly and have to translate it first to a char array

		char entry[BASE_TEXT_MAX_LENGTH];
		if (d.index() == 0) {  // std::string type
			std::strcpy(entry, std::get<std::string>(d).c_str());
			return f->write(entry, BASE_TEXT_MAX_LENGTH) == BASE_TEXT_MAX_LENGTH;
		} else {   			// All other types
			return f->write(&d, sizeof(BaseType)) == sizeof(BaseType) &&
				f->write(entry, BASE_TEXT_MAX_LENGTH - sizeof(BaseType)) == BASE_TEXT_MAX_LENGTH - sizeof(BaseType);
		}
	}

	bool deserialize_config_entry(LFSFile *f, BaseType& d) {

		// std::string is dynamically allocated so we can't read into the std::variant
		// directly and have to translate it first from a char array

		char entry[BASE_TEXT_MAX_LENGTH];
		if (d.index() == 0) {  // std::string type
			if (f->read(entry, BASE_TEXT_MAX_LENGTH) != BASE_TEXT_MAX_LENGTH)
				return false;
			d = std::string(entry);
			return true;
		} else {             // All other types
			return f->read(&d, sizeof(BaseType)) == sizeof(BaseType) &&
					f->read(entry, BASE_TEXT_MAX_LENGTH - sizeof(BaseType));
		}
	}

	void deserialize_config() {
		DEBUG_TRACE("deserialize_config");
		LFSFile *f = new LFSFile(main_filesystem, "config.dat", LFS_O_RDONLY);
		unsigned int i;

		for (i = 0; i < MAX_CONFIG_ITEMS; i++) {
			if (!deserialize_config_entry(f, m_params.at(i)))
				break;
		}

		m_is_config_valid = (i == MAX_CONFIG_ITEMS);
		delete f;
	}

	void serialize_config(ParamID param_id) override {
		DEBUG_TRACE("serial_config(%u)", (unsigned)param_id);
		LFSFile *f = new LFSFile(main_filesystem, "config.dat", LFS_O_WRONLY);
		if (f->seek((unsigned)param_id * BASE_TEXT_MAX_LENGTH) != (unsigned)param_id * BASE_TEXT_MAX_LENGTH ||
			!serialize_config_entry(f, m_params.at((unsigned)param_id))) {
			delete f;
			m_is_config_valid = false;
			throw CONFIG_STORE_CORRUPTED;
		}
		delete f;
	}

	void serialize_zone() {
		DEBUG_TRACE("serialize_zone");
		LFSFile *f = new LFSFile(main_filesystem, "zone.dat", LFS_O_WRONLY);
		m_is_zone_valid = false;
		m_is_zone_valid = f->write(&m_zone, sizeof(m_zone)) == sizeof(m_zone);
		if (!m_is_zone_valid) {
			delete f;
			throw CONFIG_STORE_CORRUPTED;
		}
		delete f;
	}

	void serialize_pass_predict() {
		DEBUG_TRACE("serialize_pass_predict");
		LFSFile *f = new LFSFile(main_filesystem, "pass_predict.dat", LFS_O_WRONLY);
		m_is_pass_predict_valid = false;
		m_is_pass_predict_valid = f->write(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict);
		delete f;
	}

public:
	void init() {
		LFSFile *f;

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

			f = new LFSFile(main_filesystem, "config.dat", LFS_O_WRONLY | LFS_O_CREAT);
			unsigned int i;
			for (i = 0; i < MAX_CONFIG_ITEMS; i++) {
				if (!serialize_config_entry(f, m_params.at(i)))
					break;
			}
			m_is_config_valid = i == MAX_CONFIG_ITEMS;
			delete f;

			DEBUG_TRACE("Created new config.data is_valid=%u", m_is_config_valid);
		}

		if (!m_is_config_valid)
			throw CONFIG_STORE_CORRUPTED; // This is a non-recoverable error

		// Read in zone file
		try {
			f = new LFSFile(main_filesystem, "zone.dat", LFS_O_CREAT | LFS_O_RDWR);
			if (f->read(&m_zone, sizeof(m_zone)) == sizeof(m_zone))
				m_is_zone_valid = true;
			delete f;
		} catch (int e) {
			DEBUG_WARN("Zone file does not exist");
		}

		// Read in pass predict file
		m_is_pass_predict_valid = false;
		try {
			f = new LFSFile(main_filesystem, "pass_predict.dat", LFS_O_CREAT | LFS_O_RDWR);
			if (f->read(&m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict))
				m_is_pass_predict_valid = true;
			delete f;
		} catch (int e) {
			DEBUG_WARN("Prepass file does not exist");
		}
	}

	bool is_valid() override {
		return m_is_config_valid;
	}

	void notify_saltwater_switch_state(bool state) override {
		// TODO
	}

	void factory_reset() override {
		main_filesystem->format();
	}

	BaseZone& read_zone(uint8_t zone_id=1) override {
		if (m_is_zone_valid && m_zone.zone_id == zone_id) {
			return m_zone;
		}
		throw CONFIG_DOES_NOT_EXIST;
	}

	BasePassPredict& read_pass_predict() override {
		if (m_is_pass_predict_valid) {
			return m_pass_predict;
		}
		throw CONFIG_DOES_NOT_EXIST;
	}

	void write_zone(BaseZone& value) override {
		m_zone = value;
		serialize_zone();
	}
	void write_pass_predict(BasePassPredict& value) override {
		m_pass_predict = value;
		serialize_pass_predict();
	}
};

#endif // __CONFIG_STORE_FS_HPP_
