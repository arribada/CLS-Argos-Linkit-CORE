#ifndef __BASE_TYPES_HPP_
#define __BASE_TYPES_HPP_

#include <cstring>
#include <string>
#include <vector>
#include <variant>
#include <ctime>

extern "C" {
	#include "previpass.h"
}

#define BASE_TEXT_MAX_LENGTH  128
#define BASE_MAX_PAYLOAD_LENGTH 0xFFF

// Offset applied to Argos frequency parameter supplied over DTE interface
#define ARGOS_FREQUENCY_OFFSET	4016200U
#define ARGOS_FREQUENCY_MULT	10000U

enum class ParamID {
	ARGOS_DECID,
	ARGOS_HEXID,
	DEVICE_MODEL,
	FW_APP_VERSION,
	LAST_TX,
	TX_COUNTER,
	BATT_SOC,
	LAST_FULL_CHARGE_DATE,
	PROFILE_NAME,
	AOP_STATUS,
	ARGOS_AOP_DATE,
	ARGOS_FREQ,
	ARGOS_POWER,
	TR_NOM,
	ARGOS_MODE,
	NTRY_PER_MESSAGE,
	DUTY_CYCLE,
	GNSS_EN,
	DLOC_ARG_NOM,
	ARGOS_DEPTH_PILE,
	GPS_CONST_SELECT,
	GLONASS_CONST_SELECT,
	GNSS_HDOPFILT_EN,
	GNSS_HDOPFILT_THR,
	GNSS_ACQ_TIMEOUT,
	GNSS_NTRY,
	UNDERWATER_EN,
	DRY_TIME_BEFORE_TX,
	SAMPLING_UNDER_FREQ,
	LB_EN,
	LB_TRESHOLD,
	LB_ARGOS_POWER,
	TR_LB,
	LB_ARGOS_MODE,
	LB_ARGOS_DUTY_CYCLE,
	LB_GNSS_EN,
	DLOC_ARG_LB,
	LB_GNSS_HDOPFILT_THR,
	LB_ARGOS_DEPTH_PILE,
	LB_GNSS_ACQ_TIMEOUT,
	SAMPLING_SURF_FREQ,
	PP_MIN_ELEVATION,
	PP_MAX_ELEVATION,
	PP_MIN_DURATION,
	PP_MAX_PASSES,
	PP_LINEAR_MARGIN,
	PP_COMP_STEP,
	GNSS_COLD_ACQ_TIMEOUT,
	GNSS_FIX_MODE,
	GNSS_DYN_MODEL,
	__NULL_PARAM = 0xFFFF
};

enum class BaseEncoding {
	DECIMAL,
	HEXADECIMAL,
	TEXT,
	DATESTRING,
	BASE64,
	BOOLEAN,
	UINT,
	FLOAT,
	DEPTHPILE,
	ARGOSMODE,
	ARGOSPOWER,
	AQPERIOD,
	ARGOSFREQ,
	GNSSFIXMODE,
	GNSSDYNMODEL,
	KEY_LIST,
	KEY_VALUE_LIST
};

enum class BaseLogDType {
	INTERNAL,
	SENSOR
};

enum class BaseArgosMode {
	OFF,
	LEGACY,
	PASS_PREDICTION,
	DUTY_CYCLE
};

enum class BaseArgosPower {
	POWER_3_MW = 1,
	POWER_40_MW,
	POWER_200_MW,
	POWER_500_MW
};

enum class BaseArgosDepthPile {
	DEPTH_PILE_1 = 1,
	DEPTH_PILE_2,
	DEPTH_PILE_3,
	DEPTH_PILE_4,
	DEPTH_PILE_8 = 8,
	DEPTH_PILE_12 = 12,
	DEPTH_PILE_16 = 16,
	DEPTH_PILE_20 = 20,
	DEPTH_PILE_24 = 24
};

enum class BaseDeltaTimeLoc {
	DELTA_T_10MIN = 1,
	DELTA_T_15MIN,
	DELTA_T_30MIN,
	DELTA_T_1HR,
	DELTA_T_2HR,
	DELTA_T_3HR,
	DELTA_T_4HR,
	DELTA_T_6HR,
	DELTA_T_12HR,
	DELTA_T_24HR
};

enum class BaseGNSSFixMode {
	FIX_2D = 1,
	FIX_3D = 2,
	AUTO = 3
};

enum class BaseGNSSDynModel {
	PORTABLE = 0,
	STATIONARY = 2,
	PEDESTRIAN = 3,
	AUTOMOTIVE = 4,
	SEA = 5,
	AIRBORNE_1G = 6,
	AIRBORNE_2G = 7,
	AIRBORNE_4G = 8,
	WRIST_WORN_WATCH = 9,
	BIKE = 10
};

enum class BaseZoneType : uint8_t {
	CIRCLE = 1
};

enum class BaseCommsVector : uint8_t {
	UNCHANGED,
	ARGOS_PREFERRED,
	CELLULAR_PREFERRED
};

struct BaseZone {
	uint8_t 	       zone_id;
	BaseZoneType       zone_type;
	bool               enable_monitoring;
	bool               enable_entering_leaving_events;
	bool			   enable_out_of_zone_detection_mode;
	bool               enable_activation_date;
	uint16_t           year;
	uint8_t            month;
	uint8_t            day;
	uint8_t            hour;
	uint8_t            minute;
	BaseCommsVector    comms_vector;
	uint32_t           delta_arg_loc_argos_seconds;
	uint32_t           delta_arg_loc_cellular_seconds;
	bool	           argos_extra_flags_enable;
	BaseArgosDepthPile argos_depth_pile;
	BaseArgosPower     argos_power;
	uint32_t           argos_time_repetition_seconds;
	BaseArgosMode      argos_mode;
	uint32_t           argos_duty_cycle;
	bool               gnss_extra_flags_enable;
	uint8_t            hdop_filter_threshold;
	uint8_t            gnss_acquisition_timeout_seconds;
	double             center_longitude_x;
	double             center_latitude_y;
	uint32_t           radius_m;
};

static bool operator==(const BaseZone& lhs, const BaseZone& rhs)
{
    return std::memcmp( &lhs, &rhs, sizeof(BaseZone) );
}

#define MAX_AOP_SATELLITE_ENTRIES		40

struct BasePassPredict {
	uint8_t num_records;
	AopSatelliteEntry_t records[MAX_AOP_SATELLITE_ENTRIES];
};

static bool operator==(const BasePassPredict& lhs, const BasePassPredict& rhs)
{
    return std::memcmp( &lhs, &rhs, sizeof(BasePassPredict) );
}

struct BaseRawData {
	// Use of a pointer and length field is permitted for encoding base64
	void *ptr;
	unsigned int length;  // Set to 0 if using string as raw data source

	// Passing string instead is generally preferred wherever possible
	std::string  str;
};

using BaseKey = std::string;
using BaseName = std::string;
using BaseConstraint = std::variant<unsigned int, int, double, std::string>;

// !!! Do not change the ordering of variants and also make sure std::string is the first entry !!!
using BaseType = std::variant<std::string, unsigned int, int, double, std::time_t, BaseRawData, BaseArgosMode, BaseArgosPower, BaseArgosDepthPile, bool, BaseGNSSFixMode, BaseGNSSDynModel>;

struct BaseMap {
	BaseName 	   name;
	BaseKey  	   key;
	BaseEncoding   encoding;
	BaseConstraint min_value;
	BaseConstraint max_value;
	std::vector<BaseConstraint> permitted_values;
	bool           is_implemented;
	bool           is_writable;
};

#endif // __BASE_TYPES_HPP_
