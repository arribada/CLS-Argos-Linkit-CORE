#ifndef __CONFIG_STORE_HPP_
#define __CONFIG_STORE_HPP_

#include <array>
#include <type_traits>
#include <iostream>
#include <ctime>
#include "base_types.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "messages.hpp"
#include "haversine.hpp"
#include "timeutils.hpp"

#define MAX_CONFIG_ITEMS  50

using namespace std::string_literals;

struct GNSSConfig {
	bool enable;
	bool hdop_filter_enable;
	unsigned int hdop_filter_threshold;
	unsigned int acquisition_timeout;
	unsigned int dloc_arg_nom;
	bool underwater_en;
	uint16_t battery_voltage;
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
	unsigned int dry_time_before_tx;
	unsigned int argos_id;
	bool underwater_en;
	double prepass_min_elevation;
	double prepass_max_elevation;
	unsigned int prepass_min_duration;
	unsigned int prepass_max_passes;
	unsigned int prepass_linear_margin;
	unsigned int prepass_comp_step;
};


class ConfigurationStore {

protected:
	static inline const std::array<BaseType,MAX_CONFIG_ITEMS> default_params { {
		/* ARGOS_DECID */ 0U,
		/* ARGOS_HEXID */ 0U,
		/* DEVICE_MODEL */ DEVICE_MODEL_NAME,
		/* FW_APP_VERSION */ FW_APP_VERSION_STR,
		/* LAST_TX */ static_cast<std::time_t>(0U),
		/* TX_COUNTER */ 0U,
		/* BATT_SOC */ 0U,
		/* LAST_FULL_CHARGE_DATE */ static_cast<std::time_t>(0U),
		/* PROFILE_NAME */ ""s,
		/* AOP_STATUS */ 0U,
		/* ARGOS_AOP_DATE */ static_cast<std::time_t>(0U),
		/* ARGOS_FREQ */ 401.6599,
		/* ARGOS_POWER */ BaseArgosPower::POWER_500_MW,
		/* TR_NOM */ 45U,
		/* ARGOS_MODE */ BaseArgosMode::OFF,
		/* NTRY_PER_MESSAGE */ 1U,
		/* DUTY_CYCLE */ 0U,
		/* GNSS_EN */ (bool)false,
		/* DLOC_ARG_NOM */ 10*60U,
		/* ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
		/* GPS_CONST_SELECT */ 0U, // Not implemented
		/* GLONASS_CONST_SELECT */ 0U, // Not implemented
		/* GNSS_HDOPFILT_EN */ (bool)false,
		/* GNSS_HDOPFILT_THR */ 2U,
		/* GNSS_ACQ_TIMEOUT */ 60U,
		/* GNSS_NTRY */ 0U, // Not implemented
		/* UNDERWATER_EN */ (bool)false,
		/* DRY_TIME_BEFORE_TX */ 1U,
		/* SAMPLING_UNDER_FREQ */ 1U,
		/* LB_EN */ (bool)false,
		/* LB_TRESHOLD */ 0U,
		/* LB_ARGOS_POWER */ BaseArgosPower::POWER_500_MW,
		/* TR_LB */ 45U,
		/* LB_ARGOS_MODE */ BaseArgosMode::OFF,
		/* LB_ARGOS_DUTY_CYCLE */ 0U,
		/* LB_GNSS_EN */ (bool)false,
		/* DLOC_ARG_LB */ 10*60U,
		/* LB_GNSS_HDOPFILT_THR */ 2U,
		/* LB_ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
		/* LB_GNSS_ACQ_TIMEOUT */ 60U,
		/* SAMPLING_SURF_FREQ */ 1U,
		/* PP_MIN_ELEVATION */ 5.0,
		/* PP_MAX_ELEVATION */ 90.0,
		/* PP_MIN_DURATION */ 300U,
		/* PP_MAX_PASSES */ 1000U,
		/* PP_LINEAR_MARGIN */ 300U,
		/* PP_COMP_STEP */ 30U,
	}};
	static inline const BaseZone default_zone = {
		/* zone_id */ 1,
		/* zone_type */ BaseZoneType::CIRCLE,
		/* enable_monitoring */ false,
		/* enable_entering_leaving_events */ false,
		/* enable_out_of_zone_detection_mode */ false,
		/* enable_activation_date */ false,
		/* year */ 2020,
		/* month */ 1,
		/* day */ 1,
		/* hour */ 0,
		/* minute */ 0,
		/* comms_vector */ BaseCommsVector::UNCHANGED,
		/* delta_arg_loc_argos_seconds */ 0,
		/* delta_arg_loc_cellular_seconds */ 0,
		/* argos_extra_flags_enable */ false,
		/* argos_depth_pile */ BaseArgosDepthPile::DEPTH_PILE_1,
		/* argos_power */ BaseArgosPower::POWER_500_MW,
		/* argos_time_repetition_seconds */ 10,
		/* argos_mode */ BaseArgosMode::OFF,
		/* argos_duty_cycle */ 0,
		/* gnss_extra_flags_enable */ false,
		/* hdop_filter_threshold */ 2,
		/* gnss_acquisition_timeout_seconds */ 45,
		/* center_longitude_x */ 0,
		/* center_latitude_y */ 0,
		/* radius_m */ 0
	};
	std::array<BaseType, MAX_CONFIG_ITEMS> m_params;
	BaseZone m_zone;
	uint8_t m_battery_level;
	uint16_t m_battery_voltage;
	GPSLogEntry m_last_gps_log_entry;
	virtual void serialize_config(ParamID) = 0;
	virtual void serialize_zone() = 0;
	virtual void update_battery_level() = 0;

public:
	ConfigurationStore() {
		m_battery_voltage = 0U;
		m_battery_level = 255U;  // Set battery level to some value that won't trigger LB mode until we get notified of a real battery level
		m_last_gps_log_entry.valid = 0; // Mark last GPS entry as invalid
	}
	virtual ~ConfigurationStore() {}
	virtual void init() = 0;
	virtual bool is_valid() = 0;
	virtual bool is_zone_valid() = 0;
	virtual void factory_reset() = 0;
	virtual BasePassPredict& read_pass_predict() = 0;
	virtual void write_pass_predict(BasePassPredict& value) = 0;
	virtual bool is_battery_level_low() = 0;

	template <typename T>
	T& read_param(ParamID param_id) {
		{ // try {
			if (is_valid()) {

				if (param_id == ParamID::BATT_SOC) {
					update_battery_level();
					m_params.at((unsigned)param_id) = (unsigned int)m_battery_level;
				}

				if (param_id == ParamID::FW_APP_VERSION) {
					m_params.at((unsigned)param_id) = FW_APP_VERSION_STR;
				}

				if constexpr (std::is_same<T, BaseType>::value) {
					return m_params.at((unsigned)param_id);
				}
				else {
					return std::get<T>(m_params.at((unsigned)param_id));
				};
			}
			else
				throw CONFIG_STORE_CORRUPTED;
		} //catch (...) {
		//	throw CONFIG_STORE_CORRUPTED;
		//}
	}

	template<typename T>
	void write_param(ParamID param_id, T& value) {
		try {
			if (is_valid()) {
				m_params.at((unsigned)param_id) = value;
				serialize_config(param_id);
			} else
				throw CONFIG_STORE_CORRUPTED;
		} catch (...) {
			throw CONFIG_STORE_CORRUPTED;
		}
	}

	BaseZone& read_zone(uint8_t zone_id=1) {
		if (is_zone_valid())
		{
			if (m_zone.zone_id == zone_id)
			{
				return m_zone;
			}
			else
			{
				DEBUG_ERROR("Zone requested: %u does not match current zone: %u", zone_id, m_zone.zone_id);
			}
		}
		else
		{
			DEBUG_ERROR("Zone is invalid");
		}
		
		throw CONFIG_DOES_NOT_EXIST;
	}

	void write_zone(BaseZone& value) {
		m_zone = value;
		serialize_zone();
	}

	void notify_gps_location(GPSLogEntry& gps_location) {
		m_last_gps_log_entry = gps_location;
	}

	bool is_zone_exclusion(void) {

		if (m_zone.enable_monitoring &&
			m_zone.enable_out_of_zone_detection_mode &&
			m_zone.zone_type == BaseZoneType::CIRCLE &&
			m_last_gps_log_entry.valid) {

			DEBUG_TRACE("ConfigurationStore::is_zone_exclusion: enabled with valid GPS fix");

			if (!m_zone.enable_activation_date || (
				convert_epochtime(m_zone.year, m_zone.month, m_zone.day, m_zone.hour, m_zone.minute, 0) <=
				convert_epochtime(m_last_gps_log_entry.year, m_last_gps_log_entry.month, m_last_gps_log_entry.day, m_last_gps_log_entry.hour, m_last_gps_log_entry.min, 0)
				)) {

				// Compute distance between two points of longitude and latitude using haversine formula
				double d_km = haversine_distance(m_zone.center_longitude_x,
												 m_zone.center_latitude_y,
												 m_last_gps_log_entry.lon,
												 m_last_gps_log_entry.lat);

				// Check if outside zone radius for exclusion parameter triggering
				if (d_km > ((double)m_zone.radius_m/1000)) {
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
		auto lb_en = read_param<bool>(ParamID::LB_EN);
		auto lb_threshold = read_param<unsigned int>(ParamID::LB_TRESHOLD);
		update_battery_level();

		gnss_config.battery_voltage = m_battery_voltage;

		if (lb_en && m_battery_level <= lb_threshold) {
			// Use LB mode which takes priority
			gnss_config.enable = read_param<bool>(ParamID::LB_GNSS_EN);
			gnss_config.dloc_arg_nom = read_param<unsigned int>(ParamID::DLOC_ARG_LB);
			gnss_config.acquisition_timeout = read_param<unsigned int>(ParamID::LB_GNSS_ACQ_TIMEOUT);
			gnss_config.hdop_filter_enable = read_param<bool>(ParamID::GNSS_HDOPFILT_EN);  // FIXME: should there be a LB_xxx variant?
			gnss_config.hdop_filter_threshold = read_param<unsigned int>(ParamID::LB_GNSS_HDOPFILT_THR);
			gnss_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
		} else if (is_zone_exclusion()) {
			gnss_config.enable = read_param<bool>(ParamID::GNSS_EN);
			gnss_config.dloc_arg_nom = m_zone.delta_arg_loc_argos_seconds == 0 ? read_param<unsigned int>(ParamID::DLOC_ARG_NOM) : m_zone.delta_arg_loc_argos_seconds;
			gnss_config.hdop_filter_enable = read_param<bool>(ParamID::GNSS_HDOPFILT_EN);
			gnss_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
			gnss_config.acquisition_timeout = read_param<unsigned int>(ParamID::GNSS_ACQ_TIMEOUT);
			gnss_config.hdop_filter_threshold = read_param<unsigned int>(ParamID::GNSS_HDOPFILT_THR);
			// Apply zone exclusion where applicable
			if (m_zone.gnss_extra_flags_enable) {
				gnss_config.acquisition_timeout = m_zone.gnss_acquisition_timeout_seconds;
				gnss_config.hdop_filter_threshold = m_zone.hdop_filter_threshold;
			}
		} else {
			// Use default params
			gnss_config.enable = read_param<bool>(ParamID::GNSS_EN);
			gnss_config.dloc_arg_nom = read_param<unsigned int>(ParamID::DLOC_ARG_NOM);
			gnss_config.acquisition_timeout = read_param<unsigned int>(ParamID::GNSS_ACQ_TIMEOUT);
			gnss_config.hdop_filter_enable = read_param<bool>(ParamID::GNSS_HDOPFILT_EN);
			gnss_config.hdop_filter_threshold = read_param<unsigned int>(ParamID::GNSS_HDOPFILT_THR);
			gnss_config.underwater_en = read_param<bool>(ParamID::UNDERWATER_EN);
		}
	}

	void get_argos_configuration(ArgosConfig& argos_config) {
		auto lb_en = read_param<bool>(ParamID::LB_EN);
		auto lb_threshold = read_param<unsigned int>(ParamID::LB_TRESHOLD);
		update_battery_level();

		std::cout << "index: " << m_params[41].index() << std::endl;

		if (lb_en && m_battery_level <= lb_threshold) {
			argos_config.tx_counter = read_param<unsigned int>(ParamID::TX_COUNTER);
			argos_config.mode = read_param<BaseArgosMode>(ParamID::LB_ARGOS_MODE);
			argos_config.depth_pile = read_param<BaseArgosDepthPile>(ParamID::LB_ARGOS_DEPTH_PILE);
			argos_config.duty_cycle = read_param<unsigned int>(ParamID::LB_ARGOS_DUTY_CYCLE);
			argos_config.frequency = read_param<double>(ParamID::ARGOS_FREQ);
			argos_config.ntry_per_message = read_param<unsigned int>(ParamID::NTRY_PER_MESSAGE);
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
		} else if (is_zone_exclusion()) {
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
			// Apply zone exclusion where applicable
			if (m_zone.argos_extra_flags_enable) {
				argos_config.mode = m_zone.argos_mode;
				argos_config.depth_pile = m_zone.argos_depth_pile;
				argos_config.duty_cycle = m_zone.argos_duty_cycle;
				argos_config.power = m_zone.argos_power;
				argos_config.tr_nom = m_zone.argos_time_repetition_seconds;
			}
		} else {
			// Use default params
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
		}
	}

	void increment_tx_counter() {
		unsigned int tx_counter = read_param<unsigned int>(ParamID::TX_COUNTER) + 1;
		write_param(ParamID::TX_COUNTER, tx_counter);
	}
};

#endif // __CONFIG_STORE_HPP_
