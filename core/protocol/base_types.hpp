#ifndef __BASE_TYPES_HPP_
#define __BASE_TYPES_HPP_

#include <cstring>
#include <string>
#include <vector>
#include <variant>
#include <ctime>


#define BASE_TEXT_MAX_LENGTH  128
#define BASE_MAX_PAYLOAD_LENGTH 0xFFF


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
	KEY_LIST,
	KEY_VALUE_LIST
};


enum class BaseArgosMode {
	OFF,
	LEGACY,
	PASS_PREDICTION,
	DUTY_CYCLE
};

enum class BaseArgosPower {
	POWER_3_MW,
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
	uint32_t           center_longitude_x;
	uint32_t           center_latitude_y;
	uint32_t           radius_m;
};

static bool operator==(const BaseZone& lhs, const BaseZone& rhs)
{
    return std::memcmp( &lhs, &rhs, sizeof(BaseZone) );
}

struct BasePassPredictRecord {
	uint8_t  satellite_hex_id;
	uint8_t  satellite_dcs_id;
	uint8_t  uplink_status;
	uint16_t year;
	uint8_t  month;
	uint8_t  day;
	uint8_t  hour;
	uint8_t  minute;
	uint8_t  second;
	uint32_t semi_major_axis;
	uint32_t orbit_inclination;
	uint32_t longitude_ascending_node;
	uint32_t ascending_node_drift;
	uint32_t orbit_period;
	uint32_t drift_semi_major_axis;
};

struct BasePassPredict {
	uint8_t num_records;
	BasePassPredictRecord records[31];
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
using BaseType = std::variant<std::string, unsigned int, int, double, std::time_t, BaseRawData, BaseArgosMode, BaseArgosPower, BaseArgosDepthPile, bool>;

struct BaseMap {
	BaseName 	   name;
	BaseKey  	   key;
	BaseEncoding   encoding;
	BaseConstraint min_value;
	BaseConstraint max_value;
	std::vector<BaseConstraint> permitted_values;
	bool           is_implemented;
};

#endif // __BASE_TYPES_HPP_
