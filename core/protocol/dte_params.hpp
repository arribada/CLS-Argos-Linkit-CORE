#pragma once

#include "base_types.hpp"

struct ParamValue {
	ParamID  param;
	BaseType value;
};

static const BaseMap param_map[] = {
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
	{ "TR_NOM", "ARP05", BaseEncoding::UINT, 45U, 1200U, {}, true, true },
	{ "ARGOS_MODE", "ARP01", BaseEncoding::ARGOSMODE, 0, 0, { 0, 1, 2, 3 }, true, true },
	{ "NTRY_PER_MESSAGE", "ARP19", BaseEncoding::UINT, 0U, 86400U, {}, true, true },
	{ "DUTY_CYCLE", "ARP18", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true, true },
	{ "GNSS_EN", "GNP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "DLOC_ARG_NOM", "ARP11", BaseEncoding::AQPERIOD, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 8 }, true, true },
	{ "ARGOS_DEPTH_PILE", "ARP16", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true, true },
	{ "GPS_CONST_SELECT", "XXX02", BaseEncoding::UINT, 0, 0, {}, false, true },  // FIXME: missing parameter key
	{ "GLONASS_CONST_SELECT", "GNP08", BaseEncoding::UINT, 0, 0, {}, false, true },
	{ "GNSS_HDOPFILT_EN", "GNP02", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "GNSS_HDOPFILT_THR", "GNP03", BaseEncoding::UINT, 2U, 15U, {}, true, true },
	{ "GNSS_ACQ_TIMEOUT", "GNP05", BaseEncoding::UINT, 10U, 600U, {}, true, true },
	{ "GNSS_NTRY", "GNP04", BaseEncoding::UINT, 0U, 0U, {}, false, true },
	{ "UNDERWATER_EN", "UNP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "DRY_TIME_BEFORE_TX", "UNP02", BaseEncoding::UINT, 1U, 1440U, {}, true, true },
	{ "SAMPLING_UNDER_FREQ", "UNP03", BaseEncoding::UINT, 1U, 1440U, {}, true, true },
	{ "LB_EN", "LBP01", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "LB_TRESHOLD", "LBP02", BaseEncoding::UINT, 0U, 100U, {}, true, true },
	{ "LB_ARGOS_POWER", "LBP03", BaseEncoding::ARGOSPOWER, 0, 0, { 0, 1, 2, 3 }, true, true },
	{ "TR_LB", "ARP06", BaseEncoding::UINT, 45U, 1200U, {}, true, true },
	{ "LB_ARGOS_MODE", "LBP04", BaseEncoding::ARGOSMODE, 0U, 0U, { 0U, 1U, 2U, 3U }, true, true },
	{ "LB_ARGOS_DUTY_CYCLE", "LBP05", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true, true },
	{ "LB_GNSS_EN", "LBP06", BaseEncoding::BOOLEAN, 0, 0, {}, true, true },
	{ "DLOC_ARG_LB", "ARP12", BaseEncoding::AQPERIOD, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 8 }, true, true },
	{ "LB_GNSS_HDOPFILT_THR", "LBP07", BaseEncoding::UINT, 2U, 15U, {}, true, true },
	{ "LB_ARGOS_DEPTH_PILE", "LBP08", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true, true },
	{ "LB_GNSS_ACQ_TIMEOUT", "LBP09", BaseEncoding::UINT, 10U, 600U, {}, true, true },
	{ "SAMPLING_SURF_FREQ", "UNP04", BaseEncoding::UINT, 1U, 1440U, {}, true, true },
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
};
