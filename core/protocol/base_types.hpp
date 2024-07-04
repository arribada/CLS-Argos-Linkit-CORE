#pragma once

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
#define KEY_LENGTH            5

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
	GNSS_HACCFILT_EN,
	GNSS_HACCFILT_THR,
	GNSS_MIN_NUM_FIXES,
	GNSS_COLD_START_RETRY_PERIOD,
	ARGOS_TIME_SYNC_BURST_EN,
	LED_MODE,
	ARGOS_TX_JITTER_EN,
	ARGOS_RX_EN,
	ARGOS_RX_MAX_WINDOW,
	ARGOS_RX_AOP_UPDATE_PERIOD,
	ARGOS_RX_COUNTER,
	ARGOS_RX_TIME,
	GNSS_ASSISTNOW_EN,
	LB_GNSS_HACCFILT_THR,
	LB_NTRY_PER_MESSAGE,
	ZONE_TYPE,
	ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE,
	ZONE_ENABLE_ACTIVATION_DATE,
	ZONE_ACTIVATION_DATE,
	ZONE_ARGOS_DEPTH_PILE,
	ZONE_ARGOS_POWER,
	ZONE_ARGOS_REPETITION_SECONDS,
	ZONE_ARGOS_MODE,
	ZONE_ARGOS_DUTY_CYCLE,
	ZONE_ARGOS_NTRY_PER_MESSAGE,
	ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS,
	ZONE_GNSS_HDOPFILT_THR,
	ZONE_GNSS_HACCFILT_THR,
	ZONE_GNSS_ACQ_TIMEOUT,
	ZONE_CENTER_LONGITUDE,
	ZONE_CENTER_LATITUDE,
	ZONE_RADIUS,
	CERT_TX_ENABLE,
	CERT_TX_PAYLOAD,
	CERT_TX_MODULATION,
	CERT_TX_REPETITION,
	HW_VERSION,
	BATT_VOLTAGE,
	ARGOS_TCXO_WARMUP_TIME,
	DEVICE_DECID,
	GNSS_TRIGGER_ON_SURFACED,
	GNSS_TRIGGER_ON_AXL_WAKEUP,
	UNDERWATER_DETECT_SOURCE,
	UNDERWATER_DETECT_THRESH,
	PH_SENSOR_ENABLE,
	PH_SENSOR_PERIODIC,
	PH_SENSOR_VALUE,
	SEA_TEMP_SENSOR_ENABLE,
	SEA_TEMP_SENSOR_PERIODIC,
	SEA_TEMP_SENSOR_VALUE,
	ALS_SENSOR_ENABLE,
	ALS_SENSOR_PERIODIC,
	ALS_SENSOR_VALUE,
	CDT_SENSOR_ENABLE,
	CDT_SENSOR_PERIODIC,
	CDT_SENSOR_CONDUCTIVITY_VALUE,
	CDT_SENSOR_DEPTH_VALUE,
	CDT_SENSOR_TEMPERATURE_VALUE,
	CAM_ENABLE,
	CAM_TRIGGER_ON_SURFACED,
	CAM_TRIGGER_ON_AXL_WAKEUP,
	CAM_PERIOD_ON,
	CAM_PERIOD_OFF,
	LB_CAM_EN,
	EXT_LED_MODE,
	AXL_SENSOR_ENABLE,
	AXL_SENSOR_PERIODIC,
	AXL_SENSOR_WAKEUP_THRESH,
	AXL_SENSOR_WAKEUP_SAMPLES,
	PRESSURE_SENSOR_ENABLE,
	PRESSURE_SENSOR_PERIODIC,
	DEBUG_OUTPUT_MODE,
	GNSS_ASSISTNOW_OFFLINE_EN,
	WCHG_STATUS,
	UW_MAX_SAMPLES,
	UW_MIN_DRY_SAMPLES,
	UW_SAMPLE_GAP,
	UW_PIN_SAMPLE_DELAY,
	UW_DIVE_MODE_ENABLE,
	UW_DIVE_MODE_START_TIME,
	UW_GNSS_DRY_SAMPLING,
	UW_GNSS_WET_SAMPLING,
	UW_GNSS_MAX_SAMPLES,
	UW_GNSS_MIN_DRY_SAMPLES,
	UW_GNSS_DETECT_THRESH,
	LB_CRITICAL_THRESH,
	PRESSURE_SENSOR_LOGGING_MODE,
	GNSS_TRIGGER_COLD_START_ON_SURFACED,
	SEA_TEMP_SENSOR_ENABLE_TX_MODE,
	SEA_TEMP_SENSOR_ENABLE_TX_MAX_SAMPLES,
	SEA_TEMP_SENSOR_ENABLE_TX_SAMPLE_PERIOD,
	PH_SENSOR_ENABLE_TX_MODE,
	PH_SENSOR_ENABLE_TX_MAX_SAMPLES,
	PH_SENSOR_ENABLE_TX_SAMPLE_PERIOD,
	ALS_SENSOR_ENABLE_TX_MODE,
	ALS_SENSOR_ENABLE_TX_MAX_SAMPLES,
	ALS_SENSOR_ENABLE_TX_SAMPLE_PERIOD,
	PRESSURE_SENSOR_ENABLE_TX_MODE,
	PRESSURE_SENSOR_ENABLE_TX_MAX_SAMPLES,
	PRESSURE_SENSOR_ENABLE_TX_SAMPLE_PERIOD,
	__PARAM_SIZE,
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
	LEDMODE,
	ZONETYPE,
	MODULATION,
	UWDETECTSOURCE,
	DEBUGMODE,
	PRESSURESENSORLOGGINGMODE,
	SENSORENABLETXMODE,
	KEY_LIST,
	KEY_VALUE_LIST
};

enum class BasePressureSensorLoggingMode {
	ALWAYS = 0,
	UW_THRESHOLD,
};

enum class BaseUnderwaterDetectSource {
	SWS = 0,
	PRESSURE_SENSOR,
	GNSS,
	SWS_GNSS
};

enum class BaseLogDType {
	INTERNAL,
	GNSS_SENSOR,
	ALS_SENSOR,
	PH_SENSOR,
	RTD_SENSOR,
	CDT_SENSOR,
	CAM_SENSOR
};

enum class BaseEraseType {
	GNSS_SENSOR = 1,
	SYSTEM,
	ALL,
	ALS_SENSOR,
	PH_SENSOR,
	RTD_SENSOR,
	CDT_SENSOR,
	CAM_SENSOR
};

enum class BaseArgosMode {
	OFF,
	PASS_PREDICTION,
	LEGACY,
	DUTY_CYCLE
};

enum class BaseArgosPower {
	POWER_3_MW = 1,
	POWER_40_MW,
	POWER_200_MW,
	POWER_500_MW,
	POWER_5_MW,
	POWER_50_MW,
	POWER_350_MW,
	POWER_750_MW,
	POWER_1000_MW,
	POWER_1500_MW
};

static inline const char *argos_power_to_string(BaseArgosPower power) {
	if (power == BaseArgosPower::POWER_3_MW)
		return "3 mW";
	if (power == BaseArgosPower::POWER_40_MW)
		return "40 mW";
	if (power == BaseArgosPower::POWER_200_MW)
		return "200 mW";
	if (power == BaseArgosPower::POWER_500_MW)
		return "500 mW";
	if (power == BaseArgosPower::POWER_5_MW)
		return "5 mW";
	if (power == BaseArgosPower::POWER_50_MW)
		return "50 mW";
	if (power == BaseArgosPower::POWER_350_MW)
		return "350 mW";
	if (power == BaseArgosPower::POWER_750_MW)
		return "750 mW";
	if (power == BaseArgosPower::POWER_1000_MW)
		return "1000 mW";
	if (power == BaseArgosPower::POWER_1500_MW)
		return "1500 mW";
	return "UNKNOWN";
}


static BaseArgosPower argos_integer_to_power(unsigned int power) {
	if (power == 3)
		return  BaseArgosPower::POWER_3_MW;
	if (power == 40)
		return BaseArgosPower::POWER_40_MW;
	if (power == 200)
		return BaseArgosPower::POWER_200_MW;
	if (power == 500)
		return BaseArgosPower::POWER_500_MW;
	if (power == 5)
		return BaseArgosPower::POWER_5_MW;
	if (power == 50)
		return BaseArgosPower::POWER_50_MW;
	if (power == 350)
		return BaseArgosPower::POWER_350_MW;
	if (power == 750)
		return BaseArgosPower::POWER_750_MW;
	if (power == 1000)
		return BaseArgosPower::POWER_1000_MW;
	if (power == 1500)
		return BaseArgosPower::POWER_1500_MW;
	return BaseArgosPower::POWER_3_MW;
}

static unsigned int argos_power_to_integer(BaseArgosPower power) {
	switch (power) {
	case BaseArgosPower::POWER_40_MW:
		return 40;
		break;
	case BaseArgosPower::POWER_500_MW:
		return 500;
		break;
	case BaseArgosPower::POWER_200_MW:
		return 200;
		break;
	case BaseArgosPower::POWER_3_MW:
		return 3;
		break;
	case BaseArgosPower::POWER_5_MW:
		return 5;
		break;
	case BaseArgosPower::POWER_50_MW:
		return 50;
		break;
	case BaseArgosPower::POWER_350_MW:
		return 350;
		break;
	case BaseArgosPower::POWER_750_MW:
		return 750;
		break;
	case BaseArgosPower::POWER_1000_MW:
		return 1000;
		break;
	case BaseArgosPower::POWER_1500_MW:
		return 1500;
		break;
	default:
		return 0;
		break;
	}
}

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

enum class BaseLEDMode {
	OFF,
	HRS_24,
	ALWAYS = 3
};

enum class BaseZoneType {
	CIRCLE = 1
};

enum class BaseDebugMode {
	UART,
	BLE_NUS
};

enum class BaseSensorEnableTxMode {
	OFF,
	ONESHOT,
	MEAN,
	MEDIAN,
};


enum class BaseArgosModulation {
	A2,
	A3,
	A4
};

static const char *argos_modulation_to_string(BaseArgosModulation m) {
	switch (m) {
	case BaseArgosModulation::A2:
		return "A2";
		break;
	case BaseArgosModulation::A3:
		return "A3";
		break;
	case BaseArgosModulation::A4:
		return "A4";
		break;
	default:
		return "UNKNOWN";
		break;
	}
}

#define MAX_AOP_SATELLITE_ENTRIES		40

struct BasePassPredict {
	unsigned int	   version_code;
	uint8_t num_records;
	AopSatelliteEntry_t records[MAX_AOP_SATELLITE_ENTRIES];
};

static bool operator==(const BasePassPredict& lhs, const BasePassPredict& rhs)
{
    return std::memcmp( &lhs, &rhs, sizeof(BasePassPredict) );
}

static bool operator==(const AopSatelliteEntry_t& lhs, const AopSatelliteEntry_t& rhs)
{
	return lhs.ascNodeDriftDeg == rhs.ascNodeDriftDeg &&
			lhs.ascNodeLongitudeDeg == rhs.ascNodeLongitudeDeg &&
			lhs.bulletin.day == rhs.bulletin.day &&
			lhs.bulletin.hour == rhs.bulletin.hour &&
			lhs.bulletin.minute == rhs.bulletin.minute &&
			lhs.bulletin.second == rhs.bulletin.second &&
			lhs.bulletin.month == rhs.bulletin.month &&
			lhs.bulletin.year == rhs.bulletin.year &&
			lhs.downlinkStatus == rhs.downlinkStatus &&
			lhs.inclinationDeg == rhs.inclinationDeg &&
			lhs.orbitPeriodMin == rhs.orbitPeriodMin &&
			lhs.satHexId == rhs.satHexId &&
			lhs.semiMajorAxisDriftMeterPerDay == rhs.semiMajorAxisDriftMeterPerDay &&
			lhs.semiMajorAxisKm == rhs.semiMajorAxisKm &&
			lhs.uplinkStatus == rhs.uplinkStatus;
}

static bool operator!=(const AopSatelliteEntry_t& lhs, const AopSatelliteEntry_t& rhs)
{
    return !(lhs == rhs);
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
using BaseType = std::variant<std::string, unsigned int, int, double, std::time_t, BaseRawData, BaseArgosMode, BaseArgosPower, BaseArgosDepthPile, bool, BaseGNSSFixMode, BaseGNSSDynModel, BaseLEDMode, BaseZoneType, BaseArgosModulation, BaseUnderwaterDetectSource, BaseDebugMode, BasePressureSensorLoggingMode, BaseSensorEnableTxMode>;

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
