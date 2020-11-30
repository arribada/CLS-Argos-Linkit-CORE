#ifndef __DTE_PARAMS_HPP_
#define __DTE_PARAMS_HPP_

#include <string>
#include <vector>
#include <variant>
#include <tuple>
#include <ctime>

#include "base_types.hpp"


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

struct ParamValue {
	ParamID  param;
	BaseType value;
};

static const BaseMap param_map[] = {
	{ "ARGOS_DECID", "IDT06", BaseEncoding::UINT, 0U, 0xFFFFFFU, {}, true },
	{ "ARGOS_HEXID", "IDT07", BaseEncoding::HEXADECIMAL, 0U, 0xFFFFFFU, {}, true },
	{ "DEVICE_MODEL", "IDT02", BaseEncoding::HEXADECIMAL, 0U, 0xFFU, {}, true },
	{ "FW_APP_VERSION", "IDT03", BaseEncoding::TEXT, 0, 0, {}, true },
	{ "LAST_TX", "ART01", BaseEncoding::DATESTRING, 0, 0, {}, true },
	{ "TX_COUNTER", "ART02", BaseEncoding::UINT, 0U, 0U, {}, true },
	{ "BATT_SOC", "POT03", BaseEncoding::UINT, 0U, 100U, {}, true },
	{ "LAST_FULL_CHARGE_DATE", "POT05", BaseEncoding::DATESTRING, 0, 0, {}, true },
	{ "PROFILE_NAME", "IDP09", BaseEncoding::TEXT, "", "", {}, true },  // FIXME: type is not specified in spreadsheet
	{ "AOP_STATUS", "XXXXX", BaseEncoding::BASE64, 0, 0, {}, true },  // FIXME: missing parameter key
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
	{ "LB_EN", "LBP01", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "LB_TRESHOLD", "LBP02", BaseEncoding::UINT, 0U, 100U, {}, true },
	{ "LB_ARGOS_POWER", "LBP03", BaseEncoding::ARGOSPOWER, 0U, 0U, { 250U, 500U, 750U, 1000U }, true },
	{ "TR_LB", "ARP06", BaseEncoding::UINT, 45U, 1200U, {}, true },
	{ "LB_ARGOS_MODE", "LBP04", BaseEncoding::ARGOSMODE, 0U, 0U, { 0U, 1U, 2U, 3U }, true },
	{ "LB_ARGOS_DUTY_CYCLE", "LBP05", BaseEncoding::HEXADECIMAL, 0U, 0xFFFFFFU, {}, true },
	{ "LB_GNSS_EN", "LBP06", BaseEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DLOC_ARG_LB", "ARP12", BaseEncoding::UINT, 0U, 0U, { 10U, 15U, 30U, 60U, 120U, 360U, 720U, 1440U }, true },
	{ "LB_GNSS_HDOPFILT_THR", "LBP07", BaseEncoding::UINT, 2U, 15U, {}, true },
	{ "LB_ARGOS_DEPTH_PILE", "LBP08", BaseEncoding::DEPTHPILE, 0U, 0U, {1U, 2U, 3U, 4U, 8U, 9U, 10U, 11U, 12U}, true },
	{ "LB_GNSS_ACQ_TIMEOUT", "LBP09", BaseEncoding::UINT, 10U, 60U, {}, true }
};

#endif // __DTE_PARAMS_HPP_
