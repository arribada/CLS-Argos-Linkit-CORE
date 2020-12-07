#ifndef __CONFIG_STORE_HPP_
#define __CONFIG_STORE_HPP_

#include <array>
#include <type_traits>
#include <iostream>
#include "base_types.hpp"
#include "error.hpp"

#define MAX_CONFIG_ITEMS  50

using namespace std::string_literals;

class ConfigurationStore {

protected:
	static inline const std::array<BaseType,MAX_CONFIG_ITEMS> default_params { {
		/* ARGOS_DECID */ 0U,
		/* ARGOS_HEXID */ 0U,
		/* DEVICE_MODEL */ 0U,
		/* FW_APP_VERSION */ "V0.1"s,
		/* LAST_TX */ static_cast<std::time_t>(0U),
		/* TX_COUNTER */ 0U,
		/* BATT_SOC */ 0U,
		/* LAST_FULL_CHARGE_DATE */ static_cast<std::time_t>(0U),
		/* PROFILE_NAME */ ""s,
		/* AOP_STATUS */ 0U,
		/* ARGOS_AOP_DATE */ static_cast<std::time_t>(0U),
		/* ARGOS_FREQ */ 399.91,
		/* ARGOS_POWER */ BaseArgosPower::POWER_750_MW,
		/* TR_NOM */ 45U,
		/* ARGOS_MODE */ BaseArgosMode::OFF,
		/* NTRY_PER_MESSAGE */ 1U,
		/* DUTY_CYCLE */ 0U,
		/* GNSS_EN */ (bool)false,
		/* DLOC_ARG_NOM */ 10U,
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
		/* LB_ARGOS_POWER */ BaseArgosPower::POWER_750_MW,
		/* TR_LB */ 45U,
		/* LB_ARGOS_MODE */ BaseArgosMode::OFF,
		/* LB_ARGOS_DUTY_CYCLE */ 0U,
		/* LB_GNSS_EN */ (bool)false,
		/* DLOC_ARG_LB */ 60U,
		/* LB_GNSS_HDOPFILT_THR */ 2U,
		/* LB_ARGOS_DEPTH_PILE */ BaseArgosDepthPile::DEPTH_PILE_1,
		/* LB_GNSS_ACQ_TIMEOUT */ 60U
	}};
	std::array<BaseType, MAX_CONFIG_ITEMS> m_params;
	virtual void serialize_config(ParamID) = 0;

public:
	virtual ~ConfigurationStore() {}
	virtual void init() = 0;
	virtual bool is_valid() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
	virtual void factory_reset() = 0;
	virtual BaseZone& read_zone(uint8_t zone_id=1) = 0;
	virtual BasePassPredict& read_pass_predict() = 0;
	virtual void write_zone(BaseZone& value) = 0;
	virtual void write_pass_predict(BasePassPredict& value) = 0;

	template <typename T>
	T& read_param(ParamID param_id) {
		if (is_valid()) {
			if constexpr (std::is_same<T, BaseType>::value) {
				return m_params.at((unsigned)param_id);
			}
			else {
				return std::get<T>(m_params.at((unsigned)param_id));
			};
		}
		else
			throw CONFIG_STORE_CORRUPTED;

	}

	template<typename T>
	void write_param(ParamID param_id, T& value) {
		if (is_valid()) {
			m_params.at((unsigned)param_id) = value;
			serialize_config(param_id);
		} else
			throw CONFIG_STORE_CORRUPTED;
	}
};

#endif // __CONFIG_STORE_HPP_
