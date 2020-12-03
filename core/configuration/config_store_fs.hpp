#ifndef __CONFIG_STORE_FS_HPP_
#define __CONFIG_STORE_FS_HPP_

#include "error.hpp"
#include "base_types.hpp"
#include "config_store.hpp"
#include "filesystem.hpp"

static constexpr BaseType default_params[MAX_CONFIG_PARAMS] = {
	/* ARGOS_DECID */ 0U,
	/* ARGOS_HEXID */ 0U,
	/* DEVICE_MODEL */ "CLSGenTracker",
	/* FW_APP_VERSION */ "V0.1",
	/* LAST_TX */ 0U,
	/* TX_COUNTER */ 0U,
	/* BATT_SOC */ 0U,
	/* LAST_FULL_CHARGE_DATE */ 0U,
	/* PROFILE_NAME */ "",
	/* AOP_STATUS */ 0U,
	/* ARGOS_AOP_DATE */ 0U,
	/* ARGOS_FREQ */ 399.91,
	/* ARGOS_POWER */ BaseArgosPower::POWER_750_MW,
	/* TR_NOM */ 45U,
	/* ARGOS_MODE */ BaseArgosMode::OFF,
	/* NTRY_PER_MESSAGE */ 1U,
	/* DUTY_CYCLE */ 0U,
	/* GNSS_EN */ false,
	/* DLOC_ARG_NOM */ 10U,
	/* ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
	/* GPS_CONST_SELECT */ 0U, // Not implemented
	/* GLONASS_CONST_SELECT */ 0U, // Not implemented
	/* GNSS_HDOPFILT_EN */ false,
	/* GNSS_HDOPFILT_THR */ 2U,
	/* GNSS_ACQ_TIMEOUT */ 60U,
	/* GNSS_NTRY */ 0U, // Not implemented
	/* UNDERWATER_EN */ false,
	/* DRY_TIME_BEFORE_TX */ 1U,
	/* SAMPLING_UNDER_FREQ */ 1U,
	/* LB_EN */ false,
	/* LB_TRESHOLD */ 0U,
	/* LB_ARGOS_POWER */ BaseArgosPower::POWER_750_MW,
	/* TR_LB */ 0U,
	/* LB_ARGOS_MODE */ BaseArgosMode::OFF,
	/* LB_ARGOS_DUTY_CYCLE */ 0U,
	/* LB_GNSS_EN */ false,
	/* DLOC_ARG_LB */ 60U,
	/* LB_GNSS_HDOPFILT_THR */ 0U,
	/* LB_ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
	/* LB_GNSS_ACQ_TIMEOUT */ 60U,
	"################################################################################################################################"
};

extern FileSystem *main_filesystem;

class LFSConfigurationStore : public ConfigurationStore {

protected:
	bool m_is_pass_predict_valid;
	bool m_is_zone_valid;
	bool m_is_config_valid;
	BaseZone m_zone;
	BasePassPredict m_pass_predict;

	void serialize_config(ParamID param_id) {
		LFSFile *f = LFSFile(main_filesystem, "config.dat", LFS_O_WRONLY);
		if (f->seek((unsigned)param_id * sizeof(BaseType)) != (unsigned)param_id * sizeof(BaseType) ||
			f->write(&m_params[(unsigned)param_id], sizeof(BaseType)) != sizeof(BaseType)) {
			delete f;
			m_is_config_valid = false;
			throw CONFIG_STORE_CORRUPTED;
		}
		delete f;
	}

	void serialize_zone() {
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

		// Read in configuration file or create new one if it doesn't not exist
		try {
			f = new LFSFile(main_filesystem, "config.dat", LFS_O_RDWR);
			if (f->read(m_params, sizeof(m_params)) == sizeof(m_params))
				m_is_config_valid = true;
			delete f;
		} catch (int e) {
			f = new LFSFile(main_filesystem, "config.dat", LFS_O_RDWR | LFS_O_CREAT);
			if (f->write(default_params, sizeof(default_params)) == sizeof(default_params))
				m_is_config_valid = true;
			delete f;
		}

		if (!m_is_config_valid)
			throw CONFIG_STORE_CORRUPTED; // This is a non-recoverable error

		// Read in zone file
		try {
			f = new LFSFile(main_filesystem, "zone.dat", LFS_O_CREAT | LFS_O_RDWR);
			if (f->read(m_zone, sizeof(m_zone)) == sizeof(m_zone))
				m_is_zone_valid = true;
			delete f;
		} catch (int e) { }

		// Read in pass predict file
		m_is_pass_predict_valid = false;
		try {
			f = new LFSFile(main_filesystem, "pass_predict.dat", LFS_O_CREAT | LFS_O_RDWR);
			if (f->read(m_pass_predict, sizeof(m_pass_predict)) == sizeof(m_pass_predict))
				m_is_pass_predict_valid = true;
			delete f;
		} catch (int e) { }
	}

	bool is_valid() {
		return m_is_config_valid;
	}

	void notify_saltwater_switch_state(bool state) {
		// TODO
	}

	void factory_reset() {
		main_filesystem->format();
	}

	BaseZone& read(uint8_t zone_id=1) {
		if (m_is_zone_valid) {
			return m_zone;
		}
		throw CONFIG_DOES_NOT_EXIST;
	}
	BasePassPredict& read() {
		if (m_is_pass_predict_valid) {
			return m_pass_predict;
		}
		throw CONFIG_DOES_NOT_EXIST;
	}

	void write(BaseZone& value) {
		m_zone = value;
		serialize_zone();
	}
	void write(BasePassPredict& value) {
		m_pass_predict = value;
		serialize_pass_predict();
	}
};

#endif // __CONFIG_STORE_FS_HPP_
