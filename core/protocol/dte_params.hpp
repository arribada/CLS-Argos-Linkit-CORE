#ifndef __DTE_PARAMS_HPP_
#define __DTE_PARAMS_HPP_

#include "base_types.hpp"

struct ParamValue {
	ParamID  param;
	BaseType value;
};

static const BaseMap param_map[] = {
	{ "ARGOS_DECID", "IDT06", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true },
	{ "ARGOS_HEXID", "IDT07", BaseEncoding::HEXADECIMAL, 0U, 0xFFFFFFU, {}, true },
	{ "DEVICE_MODEL", "IDT02", BaseEncoding::TEXT, "", "", {}, true },
	{ "FW_APP_VERSION", "IDT03", BaseEncoding::TEXT, "", "", {}, true },
	{ "LAST_TX", "ART01", BaseEncoding::DATESTRING, 0, 0, {}, true },
	{ "TX_COUNTER", "ART02", BaseEncoding::UINT, 0U, 0U, {}, true },
	{ "BATT_SOC", "POT03", BaseEncoding::UINT, 0U, 100U, {}, true },
	{ "LAST_FULL_CHARGE_DATE", "POT05", BaseEncoding::DATESTRING, 0, 0, {}, true },
	{ "PROFILE_NAME", "IDP11", BaseEncoding::TEXT, "", "", {}, true },
	{ "AOP_STATUS", "XXXXX", BaseEncoding::BASE64, 0, 0, {}, false },  // FIXME: missing parameter key
	{ "ARGOS_AOP_DATE", "ART03", BaseEncoding::DATESTRING, 0, 0, {}, true },
	{ "ARGOS_FREQ", "ARP03", BaseEncoding::FLOAT, 399.91, 401.68, {}, true },
	{ "ARGOS_POWER", "ARP04", BaseEncoding::ARGOSPOWER, 0U, 0U, { 250U, 500U, 750U, 1000U }, true },
	{ "TR_NOM", "ARP05", BaseEncoding::UINT, 45U, 1200U, {}, true },
	{ "ARGOS_MODE", "ARP01", BaseEncoding::ARGOSMODE, 0, 0, { 0, 1, 2, 3 }, true },
	{ "NTRY_PER_MESSAGE", "ARP19", BaseEncoding::UINT, 0U, 86400U, {}, true },
	{ "DUTY_CYCLE", "ARP18", BaseEncoding::HEXADECIMAL, 0U, 0xFFFFFFU, {}, true },
	{ "GNSS_EN", "GNP01", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DLOC_ARG_NOM", "ARP11", BaseEncoding::UINT, 0U, 0U, { 10U, 15U, 30U, 60U, 120U, 360U, 720U, 1440U }, true },
	{ "ARGOS_DEPTH_PILE", "ARP16", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true },
	{ "GPS_CONST_SELECT", "", BaseEncoding::DECIMAL, 0, 0, {}, false },  // FIXME: missing parameter key
	{ "GLONASS_CONST_SELECT", "GNP08", BaseEncoding::DECIMAL, 0, 0, {}, false },
	{ "GNSS_HDOPFILT_EN", "GNP02", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "GNSS_HDOPFILT_THR", "GNP03", BaseEncoding::UINT, 2U, 15U, {}, true },
	{ "GNSS_ACQ_TIMEOUT", "GNP05", BaseEncoding::UINT, 10U, 60U, {}, true },
	{ "GNSS_NTRY", "GNP04", BaseEncoding::UINT, 0U, 0U, {}, false },
	{ "UNDERWATER_EN", "UNP01", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DRY_TIME_BEFORE_TX", "UNP02", BaseEncoding::UINT, 1U, 1440U, {}, true },
	{ "SAMPLING_UNDER_FREQ", "UNP03", BaseEncoding::UINT, 1U, 1440U, {}, true },
	{ "LB_EN", "LOP01", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "LB_TRESHOLD", "LOP02", BaseEncoding::UINT, 0U, 100U, {}, true },
	{ "LB_ARGOS_POWER", "LOP03", BaseEncoding::ARGOSPOWER, 0U, 0U, { 250U, 500U, 750U, 1000U }, true },
	{ "TR_LB", "ARP06", BaseEncoding::UINT, 45U, 1200U, {}, true },
	{ "LB_ARGOS_MODE", "LOP04", BaseEncoding::ARGOSMODE, 0U, 0U, { 0U, 1U, 2U, 3U }, true },
	{ "LB_ARGOS_DUTY_CYCLE", "LOP05", BaseEncoding::HEXADECIMAL, 0U, 0xFFFFFFU, {}, true },
	{ "LB_GNSS_EN", "LOP06", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DLOC_ARG_LB", "ARP12", BaseEncoding::UINT, 0U, 0U, { 10U, 15U, 30U, 60U, 120U, 360U, 720U, 1440U }, true },
	{ "LB_GNSS_HDOPFILT_THR", "LOP07", BaseEncoding::UINT, 2U, 15U, {}, true },
	{ "LB_ARGOS_DEPTH_PILE", "LOP08", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true },
	{ "LB_GNSS_ACQ_TIMEOUT", "LOP09", BaseEncoding::UINT, 10U, 60U, {}, true },
	{ "SAMPLING_SURF_FREQ", "UNP04", BaseEncoding::UINT, 0U, 1440U, {}, true }
};

#endif // __DTE_PARAMS_HPP_
