#ifndef __BASE_TYPES_HPP_
#define __BASE_TYPES_HPP_

#define BASE_TEXT_MAX_LENGTH  128
#define BASE_MAX_PAYLOAD_LENGTH 0xFFF

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
	POWER_250_MW,
	POWER_500_MW,
	POWER_750_MW,
	POWER_1000_MW
};

enum class BaseArgosDepthPile {
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

struct BaseRawData {
	void *ptr;
	unsigned int length;
};

using BaseKey = std::string;
using BaseName = std::string;
using BaseConstraint = std::variant<unsigned int, int, double, std::string>;
using BaseType = std::variant<unsigned int, int, double, std::string, std::time_t, BaseRawData, BaseArgosMode, BaseArgosPower, BaseArgosDepthPile>;

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
