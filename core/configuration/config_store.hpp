#pragma once

#include <array>
#include <type_traits>
#include <ctime>
#include <cmath>

#include "base_types.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "messages.hpp"
#include "haversine.hpp"
#include "timeutils.hpp"
#include "pmu.hpp"
#include "sensor.hpp"
#include "wchg.hpp"
#include "service_scheduler.hpp"

#define MAX_CONFIG_ITEMS  (unsigned int)ParamID::__PARAM_SIZE

using namespace std::string_literals;

struct GNSSConfig {
	bool enable;
	bool hdop_filter_enable;
	unsigned int hdop_filter_threshold;
	bool hacc_filter_enable;
	unsigned int hacc_filter_threshold;
	unsigned int acquisition_timeout_cold_start;
	unsigned int acquisition_timeout;
	unsigned int dloc_arg_nom;
	bool underwater_en;
	uint16_t battery_voltage;
	BaseGNSSFixMode fix_mode;
	BaseGNSSDynModel dyn_model;
	bool is_out_of_zone;
	bool is_lb;
	unsigned int min_num_fixes;
	unsigned int cold_start_retry_period;
	bool assistnow_enable;
	bool trigger_on_surfaced;
	bool assistnow_offline_enable;
};

struct ArgosConfig {
	unsigned int tx_counter;
	double frequency;
	BaseArgosPower power;
	unsigned int tr_nom;
	BaseArgosMode mode;
	unsigned int ntry_per_message;
	unsigned int duty_cycle;
	BaseArgosDepthPile depth_pile;
	BaseDeltaTimeLoc delta_time_loc;
	unsigned int dry_time_before_tx;
	unsigned int argos_id;
	bool underwater_en;
	double prepass_min_elevation;
	double prepass_max_elevation;
	unsigned int prepass_min_duration;
	unsigned int prepass_max_passes;
	unsigned int prepass_linear_margin;
	unsigned int prepass_comp_step;
	bool is_out_of_zone;
	bool is_lb;
	bool time_sync_burst_en;
	bool argos_tx_jitter_en;
	bool argos_rx_en;
	unsigned int argos_rx_max_window;
	bool gnss_en;
	unsigned int argos_rx_aop_update_period;
	std::time_t last_aop_update;
	bool        cert_tx_enable;
	std::string cert_tx_payload;
	BaseArgosModulation cert_tx_modulation;
	unsigned int cert_tx_repetition;
	unsigned int argos_tcxo_warmup_time;
	unsigned int sensor_tx_enable;
};

enum class ConfigMode {
	NORMAL,
	LOW_BATTERY,
	OUT_OF_ZONE
};


class ConfigurationStore {

protected:
	static inline const unsigned int m_config_version_code = 0x1c07e800 | 0x13;
	static inline const unsigned int m_config_version_code_aop = 0x1c07e800 | 0x03;
	static inline const std::array<BaseType,MAX_CONFIG_ITEMS> default_params { {
		/* ARGOS_DECID */ 0U,
		/* ARGOS_HEXID */ 0U,
		/* DEVICE_MODEL */ DEVICE_MODEL_NAME,
		/* FW_APP_VERSION */ FW_APP_VERSION_STR,
		/* LAST_TX */ static_cast<std::time_t>(0U),
		/* TX_COUNTER */ 0U,
		/* BATT_SOC */ 0U,
		/* LAST_FULL_CHARGE_DATE */ static_cast<std::time_t>(0U),
		/* PROFILE_NAME */ "FACTORY"s,
		/* AOP_STATUS */ 0U,
		/* ARGOS_AOP_DATE */ static_cast<std::time_t>(1633646474U),
		/* ARGOS_FREQ */ 401.65,
		/* ARGOS_POWER */ BaseArgosPower::POWER_350_MW,
		/* TR_NOM */ 60U,

#if ARGOS_EXT
		/* ARGOS_MODE */ BaseArgosMode::OFF,
#else

#if MODEL_SB
		/* ARGOS_MODE */ BaseArgosMode::PASS_PREDICTION,
#else
		/* ARGOS_MODE */ BaseArgosMode::LEGACY,
#endif

#endif // ARGOS_EXT

#if MODEL_SB
		/* NTRY_PER_MESSAGE */ 6U,
#else
		/* NTRY_PER_MESSAGE */ 0U,
#endif
		/* DUTY_CYCLE */ 0U,
		/* GNSS_EN */ (bool)true,
#if MODEL_SB
		/* DLOC_ARG_NOM */ 60*60U,
#else
		/* DLOC_ARG_NOM */ 10*60U,
#endif

#if MODEL_UW
		/* ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
#else
		/* ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_16,
#endif
		/* GPS_CONST_SELECT */ 0U, // Not implemented
		/* GLONASS_CONST_SELECT */ 0U, // Not implemented
		/* GNSS_HDOPFILT_EN */ (bool)true,
		/* GNSS_HDOPFILT_THR */ 2U,
#if MODEL_UW
		/* GNSS_ACQ_TIMEOUT */ 240U,
#else
		/* GNSS_ACQ_TIMEOUT */ 120U,
#endif
		/* GNSS_NTRY */ 0U, // Not implemented
#if MODEL_UW
		/* UNDERWATER_EN */ (bool)true,
#else
		/* UNDERWATER_EN */ (bool)false,
#endif
		/* DRY_TIME_BEFORE_TX */ 1U,
		/* SAMPLING_UNDER_FREQ */ 60U,
		/* LB_EN */ (bool)false,
		/* LB_TRESHOLD */ 10U,
		/* LB_ARGOS_POWER */ BaseArgosPower::POWER_350_MW,
		/* TR_LB */ 240U,
		/* LB_ARGOS_MODE */ BaseArgosMode::LEGACY,
		/* LB_ARGOS_DUTY_CYCLE */ 0U,
		/* LB_GNSS_EN */ (bool)true,
		/* DLOC_ARG_LB */ 60*60U,
		/* LB_GNSS_HDOPFILT_THR */ 2U,
		/* LB_ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
		/* LB_GNSS_ACQ_TIMEOUT */ 120U,
		/* SAMPLING_SURF_FREQ */ 60U,
		/* PP_MIN_ELEVATION */ 15.0,
		/* PP_MAX_ELEVATION */ 90.0,
		/* PP_MIN_DURATION */ 30U,
		/* PP_MAX_PASSES */ 1000U,
		/* PP_LINEAR_MARGIN */ 300U,
		/* PP_COMP_STEP */ 10U,
		/* GNSS_COLD_ACQ_TIMEOUT */ 530U,
		/* GNSS_FIX_MODE */ BaseGNSSFixMode::AUTO,
		/* GNSS_DYN_MODEL */ BaseGNSSDynModel::PORTABLE,
		/* GNSS_HACCFILT_EN */ (bool)true,
		/* GNSS_HACCFILT_THR */ 5U,
		/* GNSS_MIN_NUM_FIXES */ 1U,
		/* GNSS_COLD_START_RETRY_PERIOD */ 60U,
		/* ARGOS_TIME_SYNC_BURST_EN */ (bool)true,
		/* LED_MODE */ BaseLEDMode::HRS_24,
		/* ARGOS_TX_JITTER_EN */ (bool)true,
		/* ARGOS_RX_EN */ (bool)true,
		/* ARGOS_RX_MAX_WINDOW */ 15U*60U,
		/* ARGOS_RX_AOP_UPDATE_PERIOD */ 90U,
		/* ARGOS_RX_COUNTER */ 0U,
		/* ARGOS_RX_TIME */ 0U,
#if 0 == NO_GPS_POWER_REG
		/* ASSIST_NOW_EN */ (bool)true,
#else
		/* ASSIST_NOW_EN */ (bool)false,
#endif
		/* LB_GNSS_HACCFILT_THR */ 5U,
		/* LB_NTRY_PER_MESSAGE */ 4U,

		/* ZONE_TYPE */ BaseZoneType::CIRCLE,
		/* ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE */ (bool)false,
		/* ZONE_ENABLE_ACTIVATION_DATE */ (bool)true,
		/* ZONE_ACTIVATION_DATE */ static_cast<std::time_t>(1577836800U), // 01/01/2020 00:00:00
		/* ZONE_ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
#if MODEL_SB
		/* ZONE_ARGOS_POWER */ BaseArgosPower::POWER_350_MW,
#else
		/* ZONE_ARGOS_POWER */ BaseArgosPower::POWER_350_MW,
#endif

#if MODEL_SB
		/* ZONE_ARGOS_REPETITION_SECONDS */ 60U,
#else
		/* ZONE_ARGOS_REPETITION_SECONDS */ 240U,
#endif

#if MODEL_SB
		/* ZONE_ARGOS_MODE */ BaseArgosMode::PASS_PREDICTION,
#else
		/* ZONE_ARGOS_MODE */ BaseArgosMode::LEGACY,
#endif

		/* ZONE_ARGOS_DUTY_CYCLE */ 0xFFFFFFU,
		/* ZONE_ARGOS_NTRY_PER_MESSAGE */ 0U,
		/* ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS */ 3600U,
		/* ZONE_GNSS_HDOPFILT_THR */ 2U,
		/* ZONE_GNSS_HACCFILT_THR */ 5U,
		/* ZONE_GNSS_ACQ_TIMEOUT */ 240U,
		/* ZONE_CENTER_LONGITUDE */ -123.3925,
		/* ZONE_CENTER_LATITUDE */ -48.8752,
		/* ZONE_RADIUS */ 1000U,
		/* CERT_TX_ENABLE */ (bool)false,
		/* CERT_TX_PAYLOAD */ "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s, // 27 bytes for long payload
		/* CERT_TX_MODULATION */ BaseArgosModulation::A2,
		/* CERT_TX_REPETITION */ 60U,
		/* HW_VERSION */ ""s,
		/* BATT_VOLTAGE */ (double)0,
		/* ARGOS_TCXO_WARMUP_TIME */ 5U,
		/* DEVICE_DECID */ 0U,
		/* GNSS_TRIGGER_ON_SURFACED */ (bool)true,
		/* GNSS_TRIGGER_ON_AXL_WAKEUP */ (bool)false,
#if MODEL_UW
		/* UNDERWATER_DETECT_SOURCE */ BaseUnderwaterDetectSource::SWS_GNSS,
#else
		/* UNDERWATER_DETECT_SOURCE */ BaseUnderwaterDetectSource::SWS,
#endif
		/* UNDERWATER_DETECT_THRESH */ (double)1.1,
		/* PH_SENSOR_ENABLE */ (bool)false,
		/* PH_SENSOR_PERIODIC */ 0U,
		/* PH_SENSOR_VALUE */ (double)0.0,
		/* SEA_TEMP_SENSOR_ENABLE */ (bool)false,
		/* SEA_TEMP_SENSOR_PERIODIC */ 0U,
		/* SEA_TEMP_SENSOR_VALUE */ (double)0.0,
		/* ALS_SENSOR_ENABLE */ (bool)false,
		/* ALS_SENSOR_PERIODIC */ 0U,
		/* ALS_SENSOR_VALUE */ (double)0.0,
		/* CDT_SENSOR_ENABLE */ (bool)false,
		/* CDT_SENSOR_PERIODIC */ 0U,
		/* CDT_SENSOR_CONDUCTIVITY */ (double)0.0,
		/* CDT_SENSOR_DEPTH */ (double)0.0,
		/* CDT_SENSOR_TEMPERATURE */ (double)0.0,
		/* EXT_LED_MODE */ BaseLEDMode::ALWAYS,
		/* AXL_SENSOR_ENABLE */ (bool)false,
		/* AXL_SENSOR_PERIODIC */ 0U,
		/* AXL_SENSOR_WAKEUP_THRESHOLD */ (double)0.0,
		/* AXL_SENSOR_WAKEUP_SAMPLES */ 5U,
		/* PRESSURE_SENSOR_ENABLE */ (bool)false,
		/* PRESSURE_SENSOR_PERIODIC */ 0U,
		/* DEBUG_OUTPUT_MODE */ BaseDebugMode::UART,
		/* GNSS_ASSISTNOW_OFFLINE_EN */ (bool)false,
		/* WCHG_STATUS */ ""s,
#if MODEL_UW
		/* UW_MAX_SAMPLES */ 10U,
#else
		/* UW_MAX_SAMPLES */ 5U,
#endif
#if MODEL_UW
		/* UW_MIN_DRY_SAMPLES */ 3U,
#else
		/* UW_MIN_DRY_SAMPLES */ 1U,
#endif
		/* UW_SAMPLE_GAP */ 1000U,
#if MODEL_UW
		/* UW_PIN_SAMPLE_DELAY */ 10U,
#else
		/* UW_PIN_SAMPLE_DELAY */ 1U,
#endif
		/* UW_DIVE_MODE_ENABLE */ (bool)true,
		/* UW_DIVE_MODE_START_TIME */ 0U,
		/* UW_GNSS_DRY_SAMPLING */ 4U * 3600U,
		/* UW_GNSS_WET_SAMPLING */ 4U * 3600U,
		/* UW_GNSS_MAX_SAMPLES */ 10U,
		/* UW_GNSS_MIN_DRY_SAMPLES */ 1U,
#if MODEL_UW
		/* UW_GNSS_DETECT_THRESH */ 2U,
#else
		/* UW_GNSS_DETECT_THRESH */ 1U,
#endif
		/* LB_CRITICAL_THRESH */ 2.8,
		/* PRESSURE_SENSOR_LOGGING_MODE */ BasePressureSensorLoggingMode::ALWAYS,
		/* GNSS_TRIGGER_COLD_START_ON_SURFACED */ (bool)false,

		/* SEA_TEMP_SENSOR_ENABLE_TX_MODE */ BaseSensorEnableTxMode::OFF,
		/* SEA_TEMP_SENSOR_ENABLE_TX_MAX_SAMPLES */ 1U,
		/* SEA_TEMP_SENSOR_ENABLE_TX_SAMPLE_PERIOD */ 1000U,
		/* PH_SENSOR_ENABLE_TX_MODE */ BaseSensorEnableTxMode::OFF,
		/* PH_SENSOR_ENABLE_TX_MAX_SAMPLES */ 1U,
		/* PH_SENSOR_SENSOR_ENABLE_TX_SAMPLE_PERIOD */ 1000U,
		/* ALS_SENSOR_ENABLE_TX_MODE */ BaseSensorEnableTxMode::OFF,
		/* ALS_SENSOR_ENABLE_TX_MAX_SAMPLES */ 1U,
		/* ALS_SENSOR_ENABLE_TX_SAMPLE_PERIOD */ 1000U,
		/* PRESSURE_SENSOR_ENABLE_TX_MODE */ BaseSensorEnableTxMode::OFF,
		/* PRESSURE_SENSOR_ENABLE_TX_MAX_SAMPLES */ 1U,
		/* PRESSURE_SENSOR_ENABLE_TX_SAMPLE_PERIOD */ 1000U,
	}};
	static inline const BasePassPredict default_prepass = {
		/* version_code */ m_config_version_code_aop,
		/* num_records */  8,
		{
			{ 0x5, 4, (SatDownlinkStatus_t)0, (SatUplinkStatus_t)2, { 2021, 10, 7, 23, 29, 36 }, 7180.188965, 98.673500, 299.226013, -25.257999, 101.033997, -0.200000 },
			{ 0x6, 4, (SatDownlinkStatus_t)0, (SatUplinkStatus_t)5, { 2021, 10, 7, 22, 41, 14 }, 6890.464844, 97.467300, 105.709999, -23.747999, 94.994003, -3.700000 },
			{ 0x8, 4, (SatDownlinkStatus_t)0, (SatUplinkStatus_t)2, { 2021, 10, 7, 23, 50, 59 }, 7225.683105, 98.983597, 331.656006, -25.497000, 101.992996, -0.900000 },
			{ 0x9, 4, (SatDownlinkStatus_t)0, (SatUplinkStatus_t)3, { 2021, 10, 7, 22, 6, 6 }, 7195.641113, 98.703400, 351.213989, -25.340000, 101.360001, -0.000000 },
			{ 0xa, 4, (SatDownlinkStatus_t)3, (SatUplinkStatus_t)3, { 2021, 10, 7, 22, 30, 43 }, 7195.528809, 98.460403, 321.191010, -25.341000, 101.358002, -0.000000 },
			{ 0xb, 4, (SatDownlinkStatus_t)3, (SatUplinkStatus_t)3, { 2021, 10, 7, 22, 58, 33 }, 7195.604004, 98.723099, 338.070007, -25.340000, 101.359001, -0.000000 },
			{ 0xc, 4, (SatDownlinkStatus_t)0, (SatUplinkStatus_t)3, { 2021, 10, 7, 23, 13, 37 }, 7226.172852, 99.176498, 299.210999, -25.497999, 102.002998, -0.600000 },
			{ 0xd, 4, (SatDownlinkStatus_t)3, (SatUplinkStatus_t)3, { 2021, 10, 7, 22, 48, 2 }, 7160.121094, 98.544098, 106.515999, -25.153000, 100.612000, -0.200000 },
		}
	};

	std::array<BaseType, MAX_CONFIG_ITEMS> m_params;
	uint8_t m_battery_level;
	uint16_t m_battery_voltage;
	bool     m_is_battery_level_low;
	GPSLogEntry m_last_gps_log_entry;
	ConfigMode  m_last_config_mode;
	virtual void serialize_config() = 0;
	virtual void update_battery_level() = 0;

private:
	static const inline unsigned int HOURS_PER_DAY = 24;
	static const inline unsigned int SECONDS_PER_MINUTE	= 60;
	static const inline unsigned int SECONDS_PER_HOUR = 3600;
	static const inline unsigned int SECONDS_PER_DAY = (SECONDS_PER_HOUR * HOURS_PER_DAY);

	BaseDeltaTimeLoc calc_delta_time_loc(unsigned int dloc_arg_nom) {
		if (dloc_arg_nom >= (24 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_24HR;
		} else if (dloc_arg_nom >= (12 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_12HR;
		} else if (dloc_arg_nom >= (6 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_6HR;
		} else if (dloc_arg_nom >= (4 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_4HR;
		} else if (dloc_arg_nom >= (3 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_3HR;
		} else if (dloc_arg_nom >= (2 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_2HR;
		} else if (dloc_arg_nom >= (1 * SECONDS_PER_HOUR)) {
			return BaseDeltaTimeLoc::DELTA_T_1HR;
		} else if (dloc_arg_nom >= (30 * SECONDS_PER_MINUTE)) {
			return BaseDeltaTimeLoc::DELTA_T_30MIN;
		} else if (dloc_arg_nom >= (15 * SECONDS_PER_MINUTE)) {
			return BaseDeltaTimeLoc::DELTA_T_15MIN;
		} else {
			return BaseDeltaTimeLoc::DELTA_T_10MIN;
		}
	}

public:
	ConfigurationStore() {
		m_last_gps_log_entry.info.valid = 0; // Mark last GPS entry as invalid
		m_last_config_mode = ConfigMode::NORMAL;
	}

	virtual ~ConfigurationStore() {}
	virtual void init() = 0;
	virtual bool is_valid() = 0;
	virtual void factory_reset() = 0;
	virtual BasePassPredict& read_pass_predict() = 0;
	virtual void write_pass_predict(BasePassPredict& value) = 0;

	template <typename T>
	T& read_param(ParamID param_id) {
		try {
			bool b_is_valid = false;

			// These parameters must always be accessible
			if (param_id == ParamID::BATT_SOC) {
				update_battery_level();
				m_params.at((unsigned)param_id) = (unsigned int)m_battery_level;
				b_is_valid = true;
			} else if (param_id == ParamID::FW_APP_VERSION) {
				m_params.at((unsigned)param_id) = FW_APP_VERSION_STR;
				b_is_valid = true;
			} else if (param_id == ParamID::HW_VERSION) {
				m_params.at((unsigned)param_id) = PMU::hardware_version();
				b_is_valid = true;
			} else if (param_id == ParamID::ARGOS_DECID) {
				b_is_valid = true;
			} else if (param_id == ParamID::ARGOS_HEXID) {
				b_is_valid = true;
			} else if (param_id == ParamID::DEVICE_MODEL) {
				m_params.at((unsigned)param_id) = DEVICE_MODEL_NAME;
				b_is_valid = true;
			} else if (param_id == ParamID::BATT_VOLTAGE) {
				update_battery_level();
				m_params.at((unsigned)param_id) = (double)m_battery_voltage / 1000.0;
				b_is_valid = true;
			} else if (param_id == ParamID::DEVICE_DECID) {
				m_params.at((unsigned)param_id) = (unsigned int)PMU::device_identifier();
				b_is_valid = true;
			} else if (param_id == ParamID::ALS_SENSOR_VALUE) {
				try {
					Sensor& s = SensorManager::find_by_name("ALS");
					m_params.at((unsigned)param_id) = s.read(1);
				} catch (...) {
					m_params.at((unsigned)param_id) = (double)std::nan("");
				}
				b_is_valid = true;
			} else if (param_id == ParamID::PH_SENSOR_VALUE) {
				try {
					Sensor& s = SensorManager::find_by_name("PH");
					m_params.at((unsigned)param_id) = s.read();
				} catch (...) {
					m_params.at((unsigned)param_id) = (double)std::nan("");
				}
				b_is_valid = true;
			} else if (param_id == ParamID::SEA_TEMP_SENSOR_VALUE) {
				try {
					Sensor& s = SensorManager::find_by_name("RTD");
					m_params.at((unsigned)param_id) = s.read();
				} catch (...) {
					m_params.at((unsigned)param_id) = (double)std::nan("");
				}
				b_is_valid = true;
			} else if (param_id == ParamID::CDT_SENSOR_CONDUCTIVITY_VALUE) {
				try {
					Sensor& s = SensorManager::find_by_name("CDT");
					m_params.at((unsigned)param_id) = s.read(0);
				} catch (...) {
					m_params.at((unsigned)param_id) = (double)std::nan("");
				}
				b_is_valid = true;
			} else if (param_id == ParamID::CDT_SENSOR_DEPTH_VALUE) {
				try {
					Sensor& s = SensorManager::find_by_name("CDT");
					m_params.at((unsigned)param_id) = s.read(1);
				} catch (...) {
					m_params.at((unsigned)param_id) = (double)std::nan("");
				}
				b_is_valid = true;
			} else if (param_id == ParamID::CDT_SENSOR_TEMPERATURE_VALUE) {
				try {
					Sensor& s = SensorManager::find_by_name("CDT");
					m_params.at((unsigned)param_id) = s.read(2);
				} catch (...) {
					m_params.at((unsigned)param_id) = (double)std::nan("");
				}
				b_is_valid = true;
			} else if (param_id == ParamID::WCHG_STATUS) {
				try {
					WirelessCharger& s = WirelessChargerManager::get_instance();
					m_params.at((unsigned)param_id) = s.get_chip_status();
				} catch (...) {
					m_params.at((unsigned)param_id) = "NOTFITTED"s;
				}
				b_is_valid = true;
			} else {
				b_is_valid = is_valid();
			}

			if (b_is_valid) {
				if constexpr (std::is_same<T, BaseType>::value) {
					return m_params.at((unsigned)param_id);
				}
				else {
					return std::get<T>(m_params.at((unsigned)param_id));
				};
			} else {
				throw CONFIG_STORE_CORRUPTED;
			}
		} catch (...) {
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	template<typename T>
	void write_param(ParamID param_id, T& value) {
		try {
			if (is_valid()) {
				m_params.at((unsigned)param_id) = value;
			} else
				throw CONFIG_STORE_CORRUPTED;
		} catch (...) {
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void save_params() {
		try {
			serialize_config();
		} catch (...) {
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	void notify_gps_location(GPSLogEntry& gps_location) {
		m_last_gps_log_entry = gps_location;
	}

	bool is_zone_exclusion(void) {

		if (read_param<bool>(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE) &&
			read_param<BaseZoneType>(ParamID::ZONE_TYPE) == BaseZoneType::CIRCLE &&
			m_last_gps_log_entry.info.valid) {

			DEBUG_TRACE("ConfigurationStore::is_zone_exclusion: enabled with valid GPS fix");

			if (!read_param<bool>(ParamID::ZONE_ENABLE_ACTIVATION_DATE) || (
				read_param<std::time_t>(ParamID::ZONE_ACTIVATION_DATE) <=
				convert_epochtime(m_last_gps_log_entry.info.year, m_last_gps_log_entry.info.month, m_last_gps_log_entry.info.day, m_last_gps_log_entry.info.hour, m_last_gps_log_entry.info.min, 0)
				)) {

				// Compute distance between two points of longitude and latitude using haversine formula
				double d_km = haversine_distance(read_param<double>(ParamID::ZONE_CENTER_LONGITUDE),
						read_param<double>(ParamID::ZONE_CENTER_LATITUDE),
												 m_last_gps_log_entry.info.lon,
												 m_last_gps_log_entry.info.lat);

				// Check if outside zone radius for exclusion parameter triggering
				if (d_km > ((double)read_param<unsigned int>(ParamID::ZONE_RADIUS) / (double)1000)) {
					DEBUG_TRACE("ConfigurationStore::is_zone_exclusion: activation criteria met, d_km = %f", d_km);
					return true;
				}
				DEBUG_TRACE("ConfigurationStore::is_zone_exclusion: activation criteria not met, d_km = %f", d_km);
				return false;
			}
		}

		DEBUG_TRACE("ConfigurationStore::is_zone_exclusion: activation criteria not met");
		return false;
	}

	void get_gnss_configuration(GNSSConfig& gnss_config) {
		auto cert_tx_enable = read_param<bool>(ParamID::CERT_TX_ENABLE);
		auto lb_en = read_param<bool>(ParamID::LB_EN);
		update_battery_level();

		gnss_config.battery_voltage = m_battery_voltage;
		gnss_config.is_out_of_zone = is_zone_exclusion();
		gnss_config.is_lb = false;

		if (lb_en && m_is_battery_level_low) {
			// Use LB mode which takes priority
			gnss_config.is_lb = true;
			gnss_config.enable = read_param<bool>(ParamID::LB_GNSS_EN);
			gnss_config.dloc_arg_nom = read_param<unsigned int>(ParamID::DLOC_ARG_LB);
			gnss_config.acquisition_timeout = read_param<unsigned int>(ParamID::LB_GNSS_ACQ_TIMEOUT);
			gnss_config.acquisition_timeout_cold_start = read_param<unsigned int>(ParamID::GNSS_COLD_ACQ_TIMEOUT);
			gnss_config.hdop_filter_enable = read_param<bool>(ParamID::GNSS_HDOPFILT_EN);
			gnss_config.hdop_filter_threshold = read_param<unsigned int>(ParamID::LB_GNSS_HDOPFILT_THR);
			gnss_config.hacc_filter_enable = read_param<bool>(ParamID::GNSS_HACCFILT_EN);
			gnss_config.hacc_filter_threshold = read_param<unsigned int>(ParamID::LB_GNSS_HACCFILT_THR);
			gnss_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			gnss_config.fix_mode = read_param<BaseGNSSFixMode>(ParamID::GNSS_FIX_MODE);
			gnss_config.dyn_model = read_param<BaseGNSSDynModel>(ParamID::GNSS_DYN_MODEL);
			gnss_config.min_num_fixes = read_param<unsigned int>(ParamID::GNSS_MIN_NUM_FIXES);
			gnss_config.cold_start_retry_period = read_param<unsigned int>(ParamID::GNSS_COLD_START_RETRY_PERIOD);
			gnss_config.assistnow_enable = read_param<bool>(ParamID::GNSS_ASSISTNOW_EN);
			gnss_config.trigger_on_surfaced = read_param<bool>(ParamID::GNSS_TRIGGER_ON_SURFACED);
			gnss_config.assistnow_offline_enable = read_param<bool>(ParamID::GNSS_ASSISTNOW_OFFLINE_EN);

			if (m_last_config_mode != ConfigMode::LOW_BATTERY) {
				DEBUG_INFO("ConfigurationStore: LOW_BATTERY mode detected");
				m_last_config_mode = ConfigMode::LOW_BATTERY;
			}

		} else if (gnss_config.is_out_of_zone) {
			gnss_config.enable = read_param<bool>(ParamID::GNSS_EN);
			gnss_config.dloc_arg_nom = read_param<unsigned int>(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS);
			gnss_config.hdop_filter_enable = read_param<bool>(ParamID::GNSS_HDOPFILT_EN);
			gnss_config.hacc_filter_enable = read_param<bool>(ParamID::GNSS_HACCFILT_EN);
			gnss_config.hacc_filter_threshold = read_param<unsigned int>(ParamID::ZONE_GNSS_HACCFILT_THR);
			gnss_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			gnss_config.acquisition_timeout = read_param<unsigned int>(ParamID::ZONE_GNSS_ACQ_TIMEOUT);
			gnss_config.acquisition_timeout_cold_start = read_param<unsigned int>(ParamID::GNSS_COLD_ACQ_TIMEOUT);
			gnss_config.hdop_filter_threshold = read_param<unsigned int>(ParamID::ZONE_GNSS_HDOPFILT_THR);
			gnss_config.fix_mode = read_param<BaseGNSSFixMode>(ParamID::GNSS_FIX_MODE);
			gnss_config.dyn_model = read_param<BaseGNSSDynModel>(ParamID::GNSS_DYN_MODEL);
			gnss_config.min_num_fixes = read_param<unsigned int>(ParamID::GNSS_MIN_NUM_FIXES);
			gnss_config.cold_start_retry_period = read_param<unsigned int>(ParamID::GNSS_COLD_START_RETRY_PERIOD);
			gnss_config.assistnow_enable = read_param<bool>(ParamID::GNSS_ASSISTNOW_EN);
			gnss_config.trigger_on_surfaced = read_param<bool>(ParamID::GNSS_TRIGGER_ON_SURFACED);
			gnss_config.assistnow_offline_enable = read_param<bool>(ParamID::GNSS_ASSISTNOW_OFFLINE_EN);

			if (m_last_config_mode != ConfigMode::OUT_OF_ZONE) {
				DEBUG_INFO("ConfigurationStore: OUT_OF_ZONE mode detected");
				m_last_config_mode = ConfigMode::OUT_OF_ZONE;
			}

		} else {
			// Use default params
			gnss_config.enable = read_param<bool>(ParamID::GNSS_EN);
			gnss_config.dloc_arg_nom = read_param<unsigned int>(ParamID::DLOC_ARG_NOM);
			gnss_config.acquisition_timeout = read_param<unsigned int>(ParamID::GNSS_ACQ_TIMEOUT);
			gnss_config.acquisition_timeout_cold_start = read_param<unsigned int>(ParamID::GNSS_COLD_ACQ_TIMEOUT);
			gnss_config.hdop_filter_enable = read_param<bool>(ParamID::GNSS_HDOPFILT_EN);
			gnss_config.hdop_filter_threshold = read_param<unsigned int>(ParamID::GNSS_HDOPFILT_THR);
			gnss_config.hacc_filter_enable = read_param<bool>(ParamID::GNSS_HACCFILT_EN);
			gnss_config.hacc_filter_threshold = read_param<unsigned int>(ParamID::GNSS_HACCFILT_THR);
			gnss_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			gnss_config.fix_mode = read_param<BaseGNSSFixMode>(ParamID::GNSS_FIX_MODE);
			gnss_config.dyn_model = read_param<BaseGNSSDynModel>(ParamID::GNSS_DYN_MODEL);
			gnss_config.min_num_fixes = read_param<unsigned int>(ParamID::GNSS_MIN_NUM_FIXES);
			gnss_config.cold_start_retry_period = read_param<unsigned int>(ParamID::GNSS_COLD_START_RETRY_PERIOD);
			gnss_config.assistnow_enable = read_param<bool>(ParamID::GNSS_ASSISTNOW_EN);
			gnss_config.trigger_on_surfaced = read_param<bool>(ParamID::GNSS_TRIGGER_ON_SURFACED);
			gnss_config.assistnow_offline_enable = read_param<bool>(ParamID::GNSS_ASSISTNOW_OFFLINE_EN);

			if (m_last_config_mode != ConfigMode::NORMAL) {
				DEBUG_INFO("ConfigurationStore: NORMAL mode detected");
				m_last_config_mode = ConfigMode::NORMAL;
			}
		}

		// Disable GNSS if certification TX is enabled
		if (cert_tx_enable) {
			DEBUG_TRACE("ConfigurationStore::get_gnss_configuration: disable GNSS as TX certification mode is set");
			gnss_config.enable = false;
		}
	}

	void get_argos_configuration(ArgosConfig& argos_config) {
		auto lb_en = read_param<bool>(ParamID::LB_EN);
		update_battery_level();

		argos_config.is_out_of_zone = is_zone_exclusion();
		argos_config.is_lb = false;

		if (lb_en && m_is_battery_level_low) {
			argos_config.is_lb = true;
			argos_config.gnss_en = read_param<bool>(ParamID::GNSS_EN);
			argos_config.last_aop_update = read_param<std::time_t>(ParamID::ARGOS_AOP_DATE);
			argos_config.argos_rx_aop_update_period = read_param<unsigned int>(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD);
			argos_config.argos_rx_max_window = read_param<unsigned int>(ParamID::ARGOS_RX_MAX_WINDOW);
			argos_config.argos_rx_en = read_param<bool>(ParamID::ARGOS_RX_EN);
			argos_config.argos_tx_jitter_en = read_param<bool>(ParamID::ARGOS_TX_JITTER_EN);
			argos_config.time_sync_burst_en = read_param<bool>(ParamID::ARGOS_TIME_SYNC_BURST_EN);
			argos_config.tx_counter = read_param<unsigned int>(ParamID::TX_COUNTER);
			argos_config.mode = read_param<BaseArgosMode>(ParamID::LB_ARGOS_MODE);
			argos_config.depth_pile = read_param<BaseArgosDepthPile>(ParamID::LB_ARGOS_DEPTH_PILE);
			argos_config.duty_cycle = read_param<unsigned int>(ParamID::LB_ARGOS_DUTY_CYCLE);
			argos_config.frequency = read_param<double>(ParamID::ARGOS_FREQ);
			argos_config.ntry_per_message = read_param<unsigned int>(ParamID::LB_NTRY_PER_MESSAGE);
			argos_config.power = read_param<BaseArgosPower>(ParamID::LB_ARGOS_POWER);
			argos_config.tr_nom = read_param<unsigned int>(ParamID::TR_LB);
			argos_config.dry_time_before_tx = read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX);
			argos_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			argos_config.argos_id = read_param<unsigned int>(ParamID::ARGOS_HEXID);
			argos_config.prepass_min_elevation = read_param<double>(ParamID::PP_MIN_ELEVATION);
			argos_config.prepass_max_elevation = read_param<double>(ParamID::PP_MAX_ELEVATION);
			argos_config.prepass_min_duration = read_param<unsigned int>(ParamID::PP_MIN_DURATION);
			argos_config.prepass_max_passes = read_param<unsigned int>(ParamID::PP_MAX_PASSES);
			argos_config.prepass_linear_margin = read_param<unsigned int>(ParamID::PP_LINEAR_MARGIN);
			argos_config.prepass_comp_step = read_param<unsigned int>(ParamID::PP_COMP_STEP);
			unsigned int delta_time_loc = read_param<unsigned int>(ParamID::DLOC_ARG_LB);
			argos_config.delta_time_loc = calc_delta_time_loc(delta_time_loc);
			if (m_last_config_mode != ConfigMode::LOW_BATTERY) {
				DEBUG_INFO("ConfigurationStore: LOW_BATTERY mode detected");
				m_last_config_mode = ConfigMode::LOW_BATTERY;
			}
		} else if (argos_config.is_out_of_zone) {
			argos_config.gnss_en = read_param<bool>(ParamID::GNSS_EN);
			argos_config.last_aop_update = read_param<std::time_t>(ParamID::ARGOS_AOP_DATE);
			argos_config.argos_rx_aop_update_period = read_param<unsigned int>(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD);
			argos_config.argos_rx_max_window = read_param<unsigned int>(ParamID::ARGOS_RX_MAX_WINDOW);
			argos_config.argos_rx_en = read_param<bool>(ParamID::ARGOS_RX_EN);
			argos_config.argos_tx_jitter_en = read_param<bool>(ParamID::ARGOS_TX_JITTER_EN);
			argos_config.time_sync_burst_en = read_param<bool>(ParamID::ARGOS_TIME_SYNC_BURST_EN);
			argos_config.tx_counter = read_param<unsigned int>(ParamID::TX_COUNTER);
			argos_config.mode = read_param<BaseArgosMode>(ParamID::ZONE_ARGOS_MODE);
			argos_config.depth_pile = read_param<BaseArgosDepthPile>(ParamID::ZONE_ARGOS_DEPTH_PILE);
			argos_config.duty_cycle = read_param<unsigned int>(ParamID::ZONE_ARGOS_DUTY_CYCLE);
			argos_config.frequency = read_param<double>(ParamID::ARGOS_FREQ);
			argos_config.ntry_per_message = read_param<unsigned int>(ParamID::ZONE_ARGOS_NTRY_PER_MESSAGE);
			argos_config.power = read_param<BaseArgosPower>(ParamID::ZONE_ARGOS_POWER);
			argos_config.tr_nom = read_param<unsigned int>(ParamID::ZONE_ARGOS_REPETITION_SECONDS);
			argos_config.dry_time_before_tx = read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX);
			argos_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			argos_config.argos_id = read_param<unsigned int>(ParamID::ARGOS_HEXID);
			argos_config.prepass_min_elevation = read_param<double>(ParamID::PP_MIN_ELEVATION);
			argos_config.prepass_max_elevation = read_param<double>(ParamID::PP_MAX_ELEVATION);
			argos_config.prepass_min_duration = read_param<unsigned int>(ParamID::PP_MIN_DURATION);
			argos_config.prepass_max_passes = read_param<unsigned int>(ParamID::PP_MAX_PASSES);
			argos_config.prepass_linear_margin = read_param<unsigned int>(ParamID::PP_LINEAR_MARGIN);
			argos_config.prepass_comp_step = read_param<unsigned int>(ParamID::PP_COMP_STEP);
			argos_config.delta_time_loc = calc_delta_time_loc(read_param<unsigned int>(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS));

			if (m_last_config_mode != ConfigMode::OUT_OF_ZONE) {
				DEBUG_INFO("ConfigurationStore: OUT_OF_ZONE mode detected");
				m_last_config_mode = ConfigMode::OUT_OF_ZONE;
			}
		} else {
			// Use default params
			argos_config.gnss_en = read_param<bool>(ParamID::GNSS_EN);
			argos_config.last_aop_update = read_param<std::time_t>(ParamID::ARGOS_AOP_DATE);
			argos_config.argos_rx_aop_update_period = read_param<unsigned int>(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD);
			argos_config.argos_rx_max_window = read_param<unsigned int>(ParamID::ARGOS_RX_MAX_WINDOW);
			argos_config.argos_rx_en = read_param<bool>(ParamID::ARGOS_RX_EN);
			argos_config.argos_tx_jitter_en = read_param<bool>(ParamID::ARGOS_TX_JITTER_EN);
			argos_config.time_sync_burst_en = read_param<bool>(ParamID::ARGOS_TIME_SYNC_BURST_EN);
			argos_config.tx_counter = read_param<unsigned int>(ParamID::TX_COUNTER);
			argos_config.mode = read_param<BaseArgosMode>(ParamID::ARGOS_MODE);
			argos_config.depth_pile = read_param<BaseArgosDepthPile>(ParamID::ARGOS_DEPTH_PILE);
			argos_config.duty_cycle = read_param<unsigned int>(ParamID::DUTY_CYCLE);
			argos_config.frequency = read_param<double>(ParamID::ARGOS_FREQ);
			argos_config.ntry_per_message = read_param<unsigned int>(ParamID::NTRY_PER_MESSAGE);
			argos_config.power = read_param<BaseArgosPower>(ParamID::ARGOS_POWER);
			argos_config.tr_nom = read_param<unsigned int>(ParamID::TR_NOM);
			argos_config.dry_time_before_tx = read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX);
			argos_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			argos_config.argos_id = read_param<unsigned int>(ParamID::ARGOS_HEXID);
			argos_config.prepass_min_elevation = read_param<double>(ParamID::PP_MIN_ELEVATION);
			argos_config.prepass_max_elevation = read_param<double>(ParamID::PP_MAX_ELEVATION);
			argos_config.prepass_min_duration = read_param<unsigned int>(ParamID::PP_MIN_DURATION);
			argos_config.prepass_max_passes = read_param<unsigned int>(ParamID::PP_MAX_PASSES);
			argos_config.prepass_linear_margin = read_param<unsigned int>(ParamID::PP_LINEAR_MARGIN);
			argos_config.prepass_comp_step = read_param<unsigned int>(ParamID::PP_COMP_STEP);
			unsigned int delta_time_loc = read_param<unsigned int>(ParamID::DLOC_ARG_NOM);
			argos_config.delta_time_loc = calc_delta_time_loc(delta_time_loc);
			if (m_last_config_mode != ConfigMode::NORMAL) {
				DEBUG_INFO("ConfigurationStore: NORMAL mode detected");
				m_last_config_mode = ConfigMode::NORMAL;
			}
		}

		// Extract certification tx params
		argos_config.cert_tx_enable = read_param<bool>(ParamID::CERT_TX_ENABLE);
		argos_config.cert_tx_modulation = read_param<BaseArgosModulation>(ParamID::CERT_TX_MODULATION);
		argos_config.cert_tx_payload = read_param<std::string>(ParamID::CERT_TX_PAYLOAD);
		argos_config.cert_tx_repetition = read_param<unsigned int>(ParamID::CERT_TX_REPETITION);
		argos_config.argos_tcxo_warmup_time = read_param<unsigned int>(ParamID::ARGOS_TCXO_WARMUP_TIME);

		// Mark GNSS disabled if certification is set
		if (argos_config.cert_tx_enable)
			argos_config.gnss_en = false;

		// Set sensor TX enable based on configuration
		argos_config.sensor_tx_enable = 0;
		if (argos_config.gnss_en) {
			argos_config.sensor_tx_enable = 0;
			argos_config.sensor_tx_enable |=
				(int)(read_param<bool>(ParamID::ALS_SENSOR_ENABLE) && read_param<BaseSensorEnableTxMode>(ParamID::ALS_SENSOR_ENABLE_TX_MODE) != BaseSensorEnableTxMode::OFF) << (int)ServiceIdentifier::ALS_SENSOR;
			argos_config.sensor_tx_enable |=
				(int)(read_param<bool>(ParamID::PRESSURE_SENSOR_ENABLE) && read_param<BaseSensorEnableTxMode>(ParamID::PRESSURE_SENSOR_ENABLE_TX_MODE) != BaseSensorEnableTxMode::OFF) << (int)ServiceIdentifier::PRESSURE_SENSOR;
			argos_config.sensor_tx_enable |=
				(int)(read_param<bool>(ParamID::SEA_TEMP_SENSOR_ENABLE) && read_param<BaseSensorEnableTxMode>(ParamID::SEA_TEMP_SENSOR_ENABLE_TX_MODE) != BaseSensorEnableTxMode::OFF) << (int)ServiceIdentifier::SEA_TEMP_SENSOR;
			argos_config.sensor_tx_enable |=
				(int)(read_param<bool>(ParamID::PH_SENSOR_ENABLE) && read_param<BaseSensorEnableTxMode>(ParamID::PH_SENSOR_ENABLE_TX_MODE) != BaseSensorEnableTxMode::OFF) << (int)ServiceIdentifier::PH_SENSOR;
		}
	}

	void increment_tx_counter() {
		unsigned int tx_counter = read_param<unsigned int>(ParamID::TX_COUNTER) + 1;
		write_param(ParamID::TX_COUNTER, tx_counter);
	}

	void increment_rx_counter() {
		unsigned int rx_counter = read_param<unsigned int>(ParamID::ARGOS_RX_COUNTER) + 1;
		write_param(ParamID::ARGOS_RX_COUNTER, rx_counter);
	}

	void increment_rx_time(unsigned int inc) {
		unsigned int rx_time = read_param<unsigned int>(ParamID::ARGOS_RX_TIME) + inc;
		write_param(ParamID::ARGOS_RX_TIME, rx_time);
	}
};
