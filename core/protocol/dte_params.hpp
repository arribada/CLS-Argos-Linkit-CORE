#ifndef __DTE_PARAMS_HPP_
#define __DTE_PARAMS_HPP_

#include <string>
#include <vector>
#include <variant>
#include <tuple>
#include <ctime>

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

enum class ParamEncoding {
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
	ARGOSPOWER
};


enum class ParamArgosMode {
	OFF,
	LEGACY,
	PASS_PREDICTION,
	DUTY_CYCLE
};

enum class ParamArgosPower {
	POWER_250_MW,
	POWER_500_MW,
	POWER_750_MW,
	POWER_1000_MW
};

enum class ParamArgosDepthPile {
	DEPTH_PILE_1 = 1,
	DEPTH_PILE_2,
	DEPTH_PILE_3,
	DEPTH_PILE_4,
	DEPTH_PILE_8 = 8,
	DEPTH_PILE_12,
	DEPTH_PILE_16,
	DEPTH_PILE_20,
	DEPTH_PILE_24
};

using ParamKey = std::string;
using ParamName = std::string;
using ParamConstraint = std::variant<int, float>;

struct ParamMap {
	ParamName 	   name;
	ParamKey  	   key;
	ParamEncoding  encoding;
	ParamConstraint min_value;
	ParamConstraint max_value;
	std::vector<ParamConstraint> permitted_values;
	bool           is_implemented;
};

struct DTEBase64 {
	void *ptr;
	unsigned int length;
};

using ParamType = std::variant<unsigned int, int, float, std::string, std::time_t, DTEBase64, ParamArgosMode, ParamArgosPower, ParamArgosDepthPile>;

struct ParamValue {
	ParamID   param;
	ParamType value;
};


static const ParamMap param_map[] = {
	{ "ARGOS_DECID", "IDT06", ParamEncoding::DECIMAL, 0, 0, {}, true },
	{ "ARGOS_HEXID", "IDT07", ParamEncoding::HEXADECIMAL, 0, 0, {}, true },
	{ "DEVICE_MODEL", "IDT02", ParamEncoding::HEXADECIMAL, 0, 0, {}, true },
	{ "FW_APP_VERSION", "IDT03", ParamEncoding::TEXT, 0, 0, {}, true },
	{ "LAST_TX", "ART01", ParamEncoding::DATESTRING, 0, 0, {}, true },
	{ "TX_COUNTER", "ART02", ParamEncoding::UINT, 0, 0, {}, true },
	{ "BATT_SOC", "POT03", ParamEncoding::UINT, 0, 100, {}, true },
	{ "LAST_FULL_CHARGE_DATE", "POT05", ParamEncoding::DATESTRING, 0, 0, {}, true },
	{ "PROFILE_NAME", "IDP09", ParamEncoding::TEXT, 0, 0, {}, true },  // FIXME: type is not specified in spreadsheet
	{ "AOP_STATUS", "", ParamEncoding::BASE64, 0, 0, {}, true },  // FIXME: missing parameter key
	{ "ARGOS_AOP_DATE", "ART03", ParamEncoding::DATESTRING, 0, 0, {}, true },
	{ "ARGOS_FREQ", "ARP03", ParamEncoding::FLOAT, 399.91f, 401.68f, {}, true },
	{ "ARGOS_POWER", "ARP04", ParamEncoding::ARGOSPOWER, 0, 0, { 250, 500, 750, 1000 }, true },
	{ "TR_NOM", "ARP05", ParamEncoding::DECIMAL, 45, 1200, {}, true },
	{ "ARGOS_MODE", "ARP01", ParamEncoding::ARGOSMODE, 0, 0, { 0, 1, 2, 3 }, true },
	{ "NTRY_PER_MESSAGE", "ARP19", ParamEncoding::DECIMAL, 0, 86400, {}, true },
	{ "DUTY_CYCLE", "ARP18", ParamEncoding::HEXADECIMAL, 0, 0xFFFFFF, {}, true },
	{ "GNSS_EN", "GNP01", ParamEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DLOC_ARG_NOM", "ARP11", ParamEncoding::DECIMAL, 0, 0, { 10, 15, 30, 60, 120, 360, 720, 1440 }, true },
	{ "ARGOS_DEPTH_PILE", "ARP16", ParamEncoding::DEPTHPILE, 1, 12, {}, true },
	{ "GPS_CONST_SELECT", "", ParamEncoding::DECIMAL, 0, 0, {}, false },  // FIXME: missing parameter key
	{ "GLONASS_CONST_SELECT", "GNP08", ParamEncoding::DECIMAL, 0, 0, {}, false },
	{ "GNSS_HDOPFILT_EN", "GNP02", ParamEncoding::BOOLEAN, 0, 0, {}, true },
	{ "GNSS_HDOPFILT_THR", "GNP03", ParamEncoding::DECIMAL, 2, 15, {}, true },
	{ "GNSS_ACQ_TIMEOUT", "GNP05", ParamEncoding::DECIMAL, 10, 60, {}, true },
	{ "GNSS_NTRY", "GNP04", ParamEncoding::DECIMAL, 0, 0, {}, false },
	{ "UNDERWATER_EN", "UNP01", ParamEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DRY_TIME_BEFORE_TX", "UNP02", ParamEncoding::DECIMAL, 1, 1440, {}, true },
	{ "SAMPLING_UNDER_FREQ", "UNP03", ParamEncoding::DECIMAL, 1, 1440, {}, true },
	{ "LB_EN", "LBP01", ParamEncoding::BOOLEAN, 0, 0, {}, true },
	{ "LB_TRESHOLD", "LBP02", ParamEncoding::DECIMAL, 0, 100, {}, true },
	{ "LB_ARGOS_POWER", "LBP03", ParamEncoding::ARGOSPOWER, 0, 0, { 250, 500, 750, 1000 }, true },
	{ "TR_LB", "ARP06", ParamEncoding::DECIMAL, 45, 1200, {}, true },
	{ "LB_ARGOS_MODE", "LBP04", ParamEncoding::ARGOSMODE, 0, 0, { 0, 1, 2, 3 }, true },
	{ "LB_ARGOS_DUTY_CYCLE", "LBP05", ParamEncoding::HEXADECIMAL, 0, 0xFFFFFF, {}, true },
	{ "LB_GNSS_EN", "LBP06", ParamEncoding::BOOLEAN, 0, 0, {}, true },
	{ "DLOC_ARG_LB", "ARP12", ParamEncoding::DECIMAL, 0, 0, {}, true },
	{ "LB_GNSS_HDOPFILT_THR", "LBP07", ParamEncoding::DECIMAL, 2, 15, {}, true },
	{ "LB_ARGOS_DEPTH_PILE", "LBP08", ParamEncoding::DEPTHPILE, 0, 0, {1, 2, 3, 4, 9, 10, 11, 12}, true },
	{ "LB_GNSS_ACQ_TIMEOUT", "LBP09", ParamEncoding::HEXADECIMAL, 10, 60, {}, true }
};

#endif // __DTE_PARAMS_HPP_
