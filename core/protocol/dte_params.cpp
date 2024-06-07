#include "dte_params.hpp"

const BaseMap param_map[] = {
	{ "ARGOS_DECID", "IDP12", BaseEncoding::UINT, 0U, 0xFFFFFFFU, {}, true, true },
	{ "ARGOS_HEXID", "IDT06", BaseEncoding::HEXADECIMAL, 0U, 0xFFFFFFFU, {}, true, true },
	{ "DEVICE_MODEL", "IDT02", BaseEncoding::TEXT, "", "", {}, true, true },
	{ "FW_APP_VERSION", "IDT03", BaseEncoding::TEXT, "", "", {}, true, false },
	{ "LAST_TX", "ART01", BaseEncoding::DATESTRING, 0, 0, {}, true, false },
	{ "TX_COUNTER", "ART02", BaseEncoding::UINT, 0U, 0U, {}, true, false },
	{ "BATT_SOC", "POT03", BaseEncoding::UINT, 0U, 100U, {}, true, false },
	{ "LAST_FULL_CHARGE_DATE", "POT05", BaseEncoding::DATESTRING, 0, 0, {}, true, false },
	{ "PROFILE_NAME", "IDP11", BaseEncoding::TEXT, "", "", {}, true, true },
	{ "AOP_STATUS", "XXX01", BaseEncoding::UINT, 0, 0, {}, false, false },  // FIXME: missing parameter key
	{ "ARGOS_AOP_DATE", "ART03", BaseEncoding::DATESTRING, 0, 0, {}, true, false },
	{ "ARGOS_FREQ", "ARP03", BaseEncoding::ARGOSFREQ, 401.6200, 401.6800, {}, true, true },
	{ "ARGOS_POWER", "ARP04", BaseEncoding::ARGOSPOWER, 0, 0, { 0, 1, 2, 3 }, true, true },
	{ "TR_NOM", "ARP05", BaseEncoding::UINT, 30U, 1200U, {}, true, true },
	{ "ARGOS_MODE", "ARP01", BaseEncoding::ARGOSMODE, 0, 0, { 0, 1, 2, 3 }, true, true },
	{ "NTRY_PER_MESSAGE", "ARP19", BaseEncoding::UINT, 0U, 86400U, {}, true, true },
	{ "DUTY_CYCLE", "ARP18", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true, true },
	{ "GNSS_EN", "GNP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "DLOC_ARG_NOM", "ARP11", BaseEncoding::AQPERIOD, 0, 0, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, true, true },
	{ "ARGOS_DEPTH_PILE", "ARP16", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true, true },
	{ "GPS_CONST_SELECT", "XXX02", BaseEncoding::UINT, 0, 0, {}, false, true },  // FIXME: missing parameter key
	{ "GLONASS_CONST_SELECT", "GNP08", BaseEncoding::UINT, 0, 0, {}, false, true },
	{ "GNSS_HDOPFILT_EN", "GNP02", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "GNSS_HDOPFILT_THR", "GNP03", BaseEncoding::UINT, 2U, 15U, {}, true, true },
	{ "GNSS_ACQ_TIMEOUT", "GNP05", BaseEncoding::UINT, 10U, 600U, {}, true, true },
	{ "GNSS_NTRY", "GNP04", BaseEncoding::UINT, 0U, 0U, {}, false, true },
	{ "UNDERWATER_EN", "UNP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "DRY_TIME_BEFORE_TX", "UNP02", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "SAMPLING_UNDER_FREQ", "UNP03", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "LB_EN", "LBP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "LB_TRESHOLD", "LBP02", BaseEncoding::UINT, 0U, 100U, {}, true, true },
	{ "LB_ARGOS_POWER", "LBP03", BaseEncoding::ARGOSPOWER, 0, 0, { 0, 1, 2, 3 }, true, true },
	{ "TR_LB", "ARP06", BaseEncoding::UINT, 30U, 1200U, {}, true, true },
	{ "LB_ARGOS_MODE", "LBP04", BaseEncoding::ARGOSMODE, 0U, 0U, { 0U, 1U, 2U, 3U }, true, true },
	{ "LB_ARGOS_DUTY_CYCLE", "LBP05", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true, true },
	{ "LB_GNSS_EN", "LBP06", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "DLOC_ARG_LB", "ARP12", BaseEncoding::AQPERIOD, 0, 0, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, true, true },
	{ "LB_GNSS_HDOPFILT_THR", "LBP07", BaseEncoding::UINT, 2U, 15U, {}, true, true },
	{ "LB_ARGOS_DEPTH_PILE", "LBP08", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true, true },
	{ "LB_GNSS_ACQ_TIMEOUT", "LBP09", BaseEncoding::UINT, 10U, 600U, {}, true, true },
	{ "SAMPLING_SURF_FREQ", "UNP04", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
    { "PP_MIN_ELEVATION", "PPP01", BaseEncoding::FLOAT, 0.0, 90.0, {}, true, true },
	{ "PP_MAX_ELEVATION", "PPP02", BaseEncoding::FLOAT, 0.0, 90.0, {}, true, true },
    { "PP_MIN_DURATION", "PPP03", BaseEncoding::UINT, 20U, 3600U, {}, true, true },
	{ "PP_MAX_PASSES", "PPP04", BaseEncoding::UINT, 1U, 10000U, {}, true, true },
	{ "PP_LINEAR_MARGIN", "PPP05", BaseEncoding::UINT, 1U, 3600U, {}, true, true },
	{ "PP_COMP_STEP", "PPP06", BaseEncoding::UINT, 1U, 1000U, {}, true, true },
	{ "GNSS_COLD_ACQ_TIMEOUT", "GNP09", BaseEncoding::UINT, 10U, 600U, {}, true, true },
	{ "GNSS_FIX_MODE", "GNP10", BaseEncoding::GNSSFIXMODE, 0U, 0U, {1U, 2U, 3U}, true, true },
	{ "GNSS_DYN_MODEL", "GNP11", BaseEncoding::GNSSDYNMODEL, 0U, 0U, {0U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U}, true, true },
	{ "GNSS_HACCFILT_EN", "GNP20", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "GNSS_HACCFILT_THR", "GNP21", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "GNSS_MIN_NUM_FIXES", "GNP22", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "GNSS_COLD_START_RETRY_PERIOD", "GNP23", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "ARGOS_TIME_SYNC_BURST_EN", "ARP30", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "LED_MODE", "LDP01", BaseEncoding::LEDMODE, 0U, 0U, {}, true, true },
	{ "ARGOS_TX_JITTER_EN", "ARP31", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "ARGOS_RX_EN", "ARP32", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "ARGOS_RX_MAX_WINDOW", "ARP33", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "ARGOS_RX_AOP_UPDATE_PERIOD", "ARP34", BaseEncoding::UINT, 0U, 0xFFFFFFFFU, {}, true, true },
	{ "ARGOS_RX_COUNTER", "ART10", BaseEncoding::UINT, 0U, 0U, {}, true, false },
	{ "ARGOS_RX_TIME", "ART11", BaseEncoding::UINT, 0U, 0U, {}, true, false },
	{ "GNSS_ASSISTNOW_EN", "GNP24", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "LB_GNSS_HACCFILT_THR", "LBP10", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "LB_NTRY_PER_MESSAGE", "LBP11", BaseEncoding::UINT, 0U, 86400U, {}, true, true },

	//////////////////////////
	// ZONE FILE
	//////////////////////////
	{ "ZONE_TYPE", "ZOP01", BaseEncoding::ZONETYPE, 0, 0, { 0U }, true, true },
	{ "ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE", "ZOP04", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "ZONE_ENABLE_ACTIVATION_DATE", "ZOP05", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "ZONE_ACTIVATION_DATE", "ZOP06", BaseEncoding::DATESTRING, 0, 0, {}, true, true },
	{ "ZONE_ARGOS_DEPTH_PILE", "ZOP08", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true, true },
	{ "ZONE_ARGOS_POWER", "ZOP09", BaseEncoding::ARGOSPOWER, 0U, 0U, { 0, 1, 2, 3 }, true, true },
	{ "ZONE_ARGOS_REPETITION_SECONDS", "ZOP10", BaseEncoding::UINT, 30U, 1200U, {}, true, true },
	{ "ZONE_ARGOS_MODE", "ZOP11", BaseEncoding::ARGOSMODE, 0U, 0U, { 0U, 1U, 2U, 3U }, true, true },
	{ "ZONE_ARGOS_DUTY_CYCLE", "ZOP12", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true, true },
	{ "ZONE_ARGOS_NTRY_PER_MESSAGE", "ZOP13", BaseEncoding::UINT, 0U, 86400U, {}, true, true },
	{ "ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS", "ZOP14", BaseEncoding::AQPERIOD, 0, 0, { 0, 1, 2, 3, 4, 5, 6, 7, 8 }, true, true },
	{ "ZONE_GNSS_HDOPFILT_THR", "ZOP15", BaseEncoding::UINT, 2U, 15U, {}, true, true },
	{ "ZONE_GNSS_HACCFILT_THR", "ZOP16", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "ZONE_GNSS_ACQ_TIMEOUT", "ZOP17", BaseEncoding::UINT, 10U, 600U, {}, true, true },
	{ "ZONE_CENTER_LONGITUDE", "ZOP18", BaseEncoding::FLOAT, -180.0, 180.0, {}, true, true },
	{ "ZONE_CENTER_LATITUDE", "ZOP19", BaseEncoding::FLOAT, -90.0, 90.0, {}, true, true },
	{ "ZONE_RADIUS", "ZOP20", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },

	//////////////////////////
	// CERTIFICATION
	//////////////////////////
	{ "CERT_TX_ENABLE", "CTP01", BaseEncoding::BOOLEAN, 0, 0, { }, true, true },
	{ "CERT_TX_PAYLOAD", "CTP02", BaseEncoding::TEXT, "", "", {}, true, true },
	{ "CERT_TX_MODULATION", "CTP03", BaseEncoding::MODULATION, 0, 0, { 0U, 1U, 2U }, true, true },
	{ "CERT_TX_REPETITION", "CTP04", BaseEncoding::UINT, 2U, 0xFFFFFFFFU, { }, true, true },

	// HW version identification
	{ "HW_VERSION", "IDT04", BaseEncoding::TEXT, "", "", {}, true, false },

	// Battery voltage
	{ "BATT_VOLTAGE", "POT06", BaseEncoding::FLOAT, 0.0, 12.0, {}, true, false },

	// TCXO warmup period
	{ "ARGOS_TCXO_WARMUP_TIME", "ARP35", BaseEncoding::UINT, 0U, 30U, {}, true, true },

	///////////////////////////
	// HORIZON support
	///////////////////////////
	{ "DEVICE_DECID", "IDT10", BaseEncoding::UINT, 0U, 0U, {}, true, false },
	{ "GNSS_TRIGGER_ON_SURFACED", "GNP25", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "GNSS_TRIGGER_ON_AXL_WAKEUP", "GNP26", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "UNDERWATER_DETECT_SOURCE", "UNP10", BaseEncoding::UWDETECTSOURCE, 0, 0, {}, true, true },
	{ "UNDERWATER_DETECT_THRESH", "UNP11", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },

	////////////////////////////
	// EXTERNAL sensors
	////////////////////////////
	{ "PH_SENSOR_ENABLE", "PHP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "PH_SENSOR_PERIODIC", "PHP02", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "PH_SENSOR_VALUE", "PHP03", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },
	{ "SEA_TEMP_SENSOR_ENABLE", "STP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "SEA_TEMP_SENSOR_PERIODIC", "STP02", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "SEA_TEMP_SENSOR_VALUE", "STP03", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },
	{ "ALS_SENSOR_ENABLE", "LTP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "ALS_SENSOR_PERIODIC", "LTP02", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "ALS_SENSOR_VALUE", "LTP03", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },
	{ "CDT_SENSOR_ENABLE", "CDP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "CDT_SENSOR_PERIODIC", "CDP02", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "CDT_SENSOR_CONDUCTIVITY", "CDP03", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },
	{ "CDT_SENSOR_DEPTH", "CDP04", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },
	{ "CDT_SENSOR_TEMPERATURE", "CDP05", BaseEncoding::FLOAT, (double)0.0, (double)0.0, {}, true, true },

	// Camera service
	{ "CAM_ENABLE", "CAM01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "CAM_TRIGGER_ON_SURFACED", "CAM02", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "CAM_TRIGGER_ON_AXL_WAKEUP", "CAM03", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "CAM_PERIOD_ON", "CAM04", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "CAM_PERIOD_OFF", "CAM05", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "LB_CAM_EN", "LBP13", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },

	// EXTERNAL LED
	{ "EXT_LED_MODE", "LDP02", BaseEncoding::LEDMODE, 0U, 0U, {}, true, true },

	// AXL
	{ "AXL_SENSOR_ENABLE", "AXP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "AXL_SENSOR_PERIODIC", "AXP02", BaseEncoding::UINT, 0U, 0U, {}, true, true },
	{ "AXL_SENSOR_WAKEUP_THRESH", "AXP03", BaseEncoding::FLOAT, (double)0.0, (double)8.0, {}, true, true },
	{ "AXL_SENSOR_WAKEUP_SAMPLES", "AXP04", BaseEncoding::UINT, 1U, 5U, {}, true, true },

	// PRESSURE
	{ "PRESSURE_SENSOR_ENABLE", "PRP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "PRESSURE_SENSOR_PERIODIC", "PRP02", BaseEncoding::UINT, 0U, 0U, {}, true, true },

	// DEBUG
	{ "DEBUG_OUTPUT_MODE", "DBP01", BaseEncoding::DEBUGMODE, 0, 0, {}, true, true },

	// GNSS ANO
	{ "GNSS_ASSISTNOW_OFFLINE_EN", "GNP27", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },

	// WCHG_STATUS
	{ "WCHG_STATUS", "WCT01", BaseEncoding::TEXT, "", "", {}, true, false },

	// Extended UW parameters
	{ "UW_MAX_SAMPLES", "UNP05", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_MIN_DRY_SAMPLES", "UNP06", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_SAMPLE_GAP", "UNP07", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_PIN_SAMPLE_DELAY", "UNP08", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },

	// Dive mode
	{ "UW_DIVE_MODE_ENABLE", "UNP12", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "UW_DIVE_MODE_START_TIME", "UNP13", BaseEncoding::UINT, 0U, 0xFFFFFFFFU, {}, true, true },

	// GNSS UW parameters
	{ "UW_GNSS_DRY_SAMPLING", "UNP14", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_GNSS_WET_SAMPLING", "UNP15", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_GNSS_MAX_SAMPLES", "UNP16", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_GNSS_MIN_DRY_SAMPLES", "UNP17", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "UW_GNSS_DETECT_THRESH", "UNP18", BaseEncoding::UINT, 1U, 7U, {}, true, true },

	// Critical battery threshold
	{ "LB_CRITICAL_THRESH", "LBP12", BaseEncoding::FLOAT, 0.0, 12.0, {}, true, true },

	// Pressure sensor logging mode
	{ "PRESSURE_SENSOR_LOGGING_MODE", "PRP03", BaseEncoding::PRESSURESENSORLOGGINGMODE, 0, 0, {}, true, true },

	// GNSS trigger cold start on surfaced
	{ "GNSS_TRIGGER_COLD_START_ON_SURFACED", "GNP28", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },


	// Sensor Argos TX options
	{ "SEA_TEMP_SENSOR_ENABLE_TX_MODE", "STP04", BaseEncoding::SENSORENABLETXMODE, 0, 0, {}, true, true },
	{ "SEA_TEMP_SENSOR_ENABLE_TX_MAX_SAMPLES", "STP05", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "SEA_TEMP_SENSOR_ENABLE_TX_SAMPLE_PERIOD", "STP06", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "PH_SENSOR_ENABLE_TX_MODE", "PHP04", BaseEncoding::SENSORENABLETXMODE, 0, 0, {}, true, true },
	{ "PH_SENSOR_ENABLE_TX_MAX_SAMPLES", "PHP05", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "PH_SENSOR_ENABLE_TX_SAMPLE_PERIOD", "PHP06", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "ALS_SENSOR_ENABLE_TX_MODE", "LTP04", BaseEncoding::SENSORENABLETXMODE, 0, 0, {}, true, true },
	{ "ALS_SENSOR_ENABLE_TX_MAX_SAMPLES", "LTP05", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "ALS_SENSOR_ENABLE_TX_SAMPLE_PERIOD", "LTP06", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "PRESSURE_SENSOR_ENABLE_TX_MODE", "PRP04", BaseEncoding::SENSORENABLETXMODE, 0, 0, {}, true, true },
	{ "PRESSURE_SENSOR_ENABLE_TX_MAX_SAMPLES", "PRP05", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
	{ "PRESSURE_SENSOR_ENABLE_TX_SAMPLE_PERIOD", "PRP06", BaseEncoding::UINT, 1U, 0xFFFFFFFFU, {}, true, true },
};

const size_t param_map_size = sizeof(param_map) / sizeof(param_map[0]);
