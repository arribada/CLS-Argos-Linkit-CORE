#include <ios>
#include <iomanip>
#include <ctime>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>

#include "debug.hpp"

#include "base64.hpp"

#include "dte_params.hpp"
#include "dte_commands.hpp"
#include "error.hpp"
#include "bitpack.hpp"
#include "timeutils.hpp"

using namespace std::literals::string_literals;


class ZoneCodec {
private:
	static unsigned int decode_arg_loc_argos(unsigned int x) {
		switch (x) {
		case 1:
			return 7 * 60;
			break;
		case 2:
			return 15 * 60;
			break;
		case 3:
			return 30 * 60;
			break;
		case 4:
			return 1 * 60 * 60;
			break;
		case 5:
			return 2 * 60 * 60;
			break;
		case 6:
			return 3 * 60 * 60;
			break;
		case 7:
			return 4 * 60 * 60;
			break;
		case 8:
			return 6 * 60 * 60;
			break;
		case 9:
			return 12 * 60 * 60;
			break;
		case 10:
			return 24 * 60 * 60;
			break;
		
		default:
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%lu)", __FUNCTION__, x);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}

	}

	static BaseArgosDepthPile decode_depth_pile(unsigned int x) {
		switch (x) {
		case 1:
			return BaseArgosDepthPile::DEPTH_PILE_1;
			break;
		case 2:
			return BaseArgosDepthPile::DEPTH_PILE_2;
			break;
		case 4:
			return BaseArgosDepthPile::DEPTH_PILE_4;
			break;
		case 8:
			return BaseArgosDepthPile::DEPTH_PILE_8;
			break;
		case 9:
			return BaseArgosDepthPile::DEPTH_PILE_12;
			break;
		case 10:
			return BaseArgosDepthPile::DEPTH_PILE_16;
			break;
		case 11:
			return BaseArgosDepthPile::DEPTH_PILE_20;
			break;
		case 12:
			return BaseArgosDepthPile::DEPTH_PILE_24;
			break;
		
		default:
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%lu)", __FUNCTION__, x);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static BaseCommsVector decode_comms_vector(unsigned int x) {
		switch (x) {
		case 0:
			return BaseCommsVector::UNCHANGED;
		case 1:
			return BaseCommsVector::ARGOS_PREFERRED;
		case 2:
			return BaseCommsVector::CELLULAR_PREFERRED;
		default:
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%lu)", __FUNCTION__, x);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static unsigned int encode_depth_pile(BaseArgosDepthPile x) {
		switch (x) {
		case BaseArgosDepthPile::DEPTH_PILE_1:
			return 1;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_2:
			return 2;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_3:
			return 3;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_4:
			return 4;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_8:
			return 8;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_12:
			return 9;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_16:
			return 10;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_20:
			return 11;
			break;
		case BaseArgosDepthPile::DEPTH_PILE_24:
			return 12;
			break;

		default:
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%d)", __FUNCTION__, x);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static unsigned int encode_arg_loc_argos(unsigned int x) {
		switch (x) {
		case 7 * 60:
			return 1;
			break;
		case 15 * 60:
			return 2;
			break;
		case 30 * 60:
			return 3;
			break;
		case 1 * 60 * 60:
			return 4;
			break;
		case 2 * 60 * 60:
			return 5;
			break;
		case 3 * 60 * 60:
			return 6;
			break;
		case 4 * 60 * 60:
			return 7;
			break;
		case 6 * 60 * 60:
			return 8;
			break;
		case 12 * 60 * 60:
			return 9;
			break;
		case 24 * 60 * 60:
			return 10;
			break;
		
		default:
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%lu)", __FUNCTION__, x);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static double convert_longitude_to_decimal(uint32_t longitude) {
		double d;
		if (longitude < (360*1E4)) { // E
			d = longitude * 5*1E-5;
		} else { // W
			d = ((longitude - (360*1E4)) * -5*1E-5);
		}
		return d;
	}

	static uint32_t convert_decimal_to_longitude(double longitude) {
		uint32_t l;
		if (longitude > 0) { // E
			l = (uint32_t)(2*1E4 * longitude);
		} else { // W
			l = (uint32_t)((-2*1E4 * longitude) + (360*1E4));
		}
		return l;
	}

	static double convert_latitude_to_decimal(uint32_t latitude) {
		double d;
		if (latitude < (180*1E4)) { // S
			d = latitude * (-5 * 1E-5);
		} else { // N
			d = (latitude - (180*1E4)) * (5 * 1E-5);
		}
		return d;
	}

	static uint32_t convert_decimal_to_latitude(double latitude) {
		uint32_t l;
		if (latitude < 0) { // S
			l = (uint32_t)(-2E4 * latitude);
		} else { // N
			l = (uint32_t)((2E4 * latitude) + (180*1E4));
		}
		return l;
	}

public:
	static void decode(const std::string& data, BaseZone& zone) {
		unsigned int base_pos = 0;
		EXTRACT_BITS(zone.zone_id, data, base_pos, 7);
		EXTRACT_BITS_CAST(BaseZoneType, zone.zone_type, data, base_pos, 1);
		EXTRACT_BITS(zone.enable_monitoring, data, base_pos, 1);
		EXTRACT_BITS(zone.enable_entering_leaving_events, data, base_pos, 1);
		EXTRACT_BITS(zone.enable_out_of_zone_detection_mode, data, base_pos, 1);
		EXTRACT_BITS(zone.enable_activation_date, data, base_pos, 1);
		EXTRACT_BITS(zone.year, data, base_pos, 5);
		zone.year += 2020;
		EXTRACT_BITS(zone.month, data, base_pos, 4);
		EXTRACT_BITS(zone.day, data, base_pos, 5);
		EXTRACT_BITS(zone.hour, data, base_pos, 5);
		EXTRACT_BITS(zone.minute, data, base_pos, 6);
		unsigned int comms_vector;
		EXTRACT_BITS(comms_vector, data, base_pos, 2);
		zone.comms_vector = decode_comms_vector(comms_vector);
		EXTRACT_BITS(zone.delta_arg_loc_argos_seconds, data, base_pos, 4);
		zone.delta_arg_loc_argos_seconds = decode_arg_loc_argos(zone.delta_arg_loc_argos_seconds);
		EXTRACT_BITS(zone.delta_arg_loc_cellular_seconds, data, base_pos, 7);  // Not used
		EXTRACT_BITS(zone.argos_extra_flags_enable, data, base_pos, 1);
		unsigned int argos_depth_pile;
		EXTRACT_BITS(argos_depth_pile, data, base_pos, 4);
		zone.argos_depth_pile = decode_depth_pile(argos_depth_pile);
		EXTRACT_BITS_CAST(BaseArgosPower, zone.argos_power, data, base_pos, 2);
		EXTRACT_BITS(zone.argos_time_repetition_seconds, data, base_pos, 7);
		zone.argos_time_repetition_seconds *= 10;
		EXTRACT_BITS_CAST(BaseArgosMode, zone.argos_mode, data, base_pos, 2);
		EXTRACT_BITS(zone.argos_duty_cycle, data, base_pos, 24);
		EXTRACT_BITS(zone.gnss_extra_flags_enable, data, base_pos, 1);
		EXTRACT_BITS(zone.hdop_filter_threshold, data, base_pos, 4);
		EXTRACT_BITS(zone.gnss_acquisition_timeout_seconds, data, base_pos, 8);
		uint32_t center_longitude_x;
		EXTRACT_BITS(center_longitude_x, data, base_pos, 23);
		zone.center_longitude_x = convert_longitude_to_decimal(center_longitude_x);
		uint32_t center_latitude_y;
		EXTRACT_BITS(center_latitude_y, data, base_pos, 22);
		zone.center_latitude_y = convert_latitude_to_decimal(center_latitude_y);
		EXTRACT_BITS(zone.radius_m, data, base_pos, 12);
	}

	static void encode(const BaseZone& zone, std::string &data) {
		unsigned int base_pos = 0;

		// Zero out the data buffer to the required number of bytes -- this will round up to
		// the nearest number of bytes and zero all bytes before encoding
		unsigned int total_bits = 160;
		data.assign((total_bits + 4) / 8, 0);

		PACK_BITS(zone.zone_id, data, base_pos, 7);
		PACK_BITS_CAST(uint32_t, zone.zone_type, data, base_pos, 1);
		PACK_BITS(zone.enable_monitoring, data, base_pos, 1);
		PACK_BITS(zone.enable_entering_leaving_events, data, base_pos, 1);
		PACK_BITS(zone.enable_out_of_zone_detection_mode, data, base_pos, 1);
		PACK_BITS(zone.enable_activation_date, data, base_pos, 1);
		PACK_BITS((zone.year - 2020), data, base_pos, 5);
		PACK_BITS(zone.month, data, base_pos, 4);
		PACK_BITS(zone.day, data, base_pos, 5);
		PACK_BITS(zone.hour, data, base_pos, 5);
		PACK_BITS(zone.minute, data, base_pos, 6);
		PACK_BITS_CAST(uint32_t, zone.comms_vector, data, base_pos, 2);
		unsigned int delta_arg_loc_argos_seconds = encode_arg_loc_argos(zone.delta_arg_loc_argos_seconds);
		PACK_BITS(delta_arg_loc_argos_seconds, data, base_pos, 4);
		PACK_BITS(0U, data, base_pos, 7); // zone.delta_arg_loc_cellular_seconds not used!
		PACK_BITS(zone.argos_extra_flags_enable, data, base_pos, 1);
		unsigned int argos_depth_pile;
		argos_depth_pile = encode_depth_pile(zone.argos_depth_pile);
		PACK_BITS(argos_depth_pile, data, base_pos, 4);
		PACK_BITS_CAST(uint32_t, zone.argos_power, data, base_pos, 2);
		PACK_BITS((zone.argos_time_repetition_seconds / 10), data, base_pos, 7);
		PACK_BITS_CAST(uint32_t, zone.argos_mode, data, base_pos, 2);
		PACK_BITS(zone.argos_duty_cycle, data, base_pos, 24);
		PACK_BITS(zone.gnss_extra_flags_enable, data, base_pos, 1);
		PACK_BITS(zone.hdop_filter_threshold, data, base_pos, 4);
		PACK_BITS(zone.gnss_acquisition_timeout_seconds, data, base_pos, 8);
		uint32_t center_longitude_x = convert_decimal_to_longitude(zone.center_longitude_x);
		PACK_BITS(center_longitude_x, data, base_pos, 23);
		uint32_t center_latitude_y = convert_decimal_to_latitude(zone.center_latitude_y);
		PACK_BITS(center_latitude_y, data, base_pos, 22);
		PACK_BITS(zone.radius_m, data, base_pos, 12);
	}
};

// IMPORTANT NOTE: This codec relies upon the fact that AllCast message types C7 and 5F come _after_
// message type BE.

class PassPredictCodec {
private:

	static SatDownlinkStatus_t convert_dl_operating_status(uint8_t status) {
		switch (status) {
		case 1:
			return SAT_DNLK_ON_WITH_A4;
		case 3:
			return SAT_DNLK_ON_WITH_A3;
		default:
			return SAT_DNLK_OFF;
		}
	}

	static SatUplinkStatus_t convert_ul_operating_status(uint8_t status) {
		switch (status) {
		case 0:
			return SAT_UPLK_ON_WITH_A3;
		case 1:
			return SAT_UPLK_ON_WITH_NEO;
		case 2:
			return SAT_UPLK_ON_WITH_A4;
		default:
			return SAT_UPLK_OFF;
		}
	}

	static int find_prepass_record_by_hex_id(uint8_t hex_id, BasePassPredict& pass_predict) {
		for (unsigned int i = 0; i < pass_predict.num_records; i++) {
			if (pass_predict.records[i].satHexId == hex_id)
				return i;
		}
		return -1;
	}

	static void allcast_constellation_status_decode(const std::string& data, unsigned int &pos, bool type_a, BasePassPredict& pass_predict) {
		uint8_t num_operational_satellites;
		EXTRACT_BITS(num_operational_satellites, data, pos, 4);
		DEBUG_TRACE("allcast_constellation_status_decode: num_operational_satellites: %u", num_operational_satellites);
		for (uint8_t i = 0; i < num_operational_satellites; i++) {
			uint8_t hex_id;
			uint8_t a_dcs;
			uint8_t dl_status;
			uint8_t ul_status;
			EXTRACT_BITS(hex_id, data, pos, 4);
			EXTRACT_BITS(a_dcs, data, pos, 4);
			if (type_a) {
				EXTRACT_BITS(dl_status, data, pos, 2);
				EXTRACT_BITS(ul_status, data, pos, 2);
			} else {
				EXTRACT_BITS(dl_status, data, pos, 1);
				EXTRACT_BITS(ul_status, data, pos, 3);
			}
			(void)a_dcs;
			DEBUG_TRACE("allcast_constellation_status_decode: sat=%u hex_id=%01x a_dcs=%01x dl_status=%01x ul_status=%01x", i,
						hex_id, a_dcs, dl_status, ul_status);
			int record_index = find_prepass_record_by_hex_id(hex_id, pass_predict);
			if (record_index >= 0) {
				DEBUG_TRACE("allcast_constellation_status_decode: updating status for prepass record %u", record_index);
				pass_predict.records[record_index].downlinkStatus = convert_dl_operating_status(dl_status);
				pass_predict.records[record_index].uplinkStatus = convert_ul_operating_status(ul_status);
			} else {
				DEBUG_WARN("allcast_constellation_status_decode: existing prepass entry not found");
			}
		}

#if 0 // FIXME: the example messages supplied by CLS are incorrectly encoded and are not a multiple of 8 bits
		// Skip reserved field if number of status words is even
		if ((num_operational_satellites & 1) == 0) {
			uint8_t reserved;
			EXTRACT_BITS(reserved, data, pos, 4);
			(void)reserved;
		}
#endif
	}

	static void allcast_sat_orbit_params_decode(const std::string& data, unsigned int &pos, AopSatelliteEntry_t &aop_entry) {
		uint32_t working, day_of_year;

		// 4 bits sat hex ID
		EXTRACT_BITS(aop_entry.satHexId, data, pos, 4);

		DEBUG_TRACE("allcast_sat_orbit_params_decode: satHexId=%01x", aop_entry.satHexId);

		// 2 bites bulletin type (not used)
		EXTRACT_BITS(working, data, pos, 2); // Type of bulletin

		// 44 bits of date
		aop_entry.bulletin.year = 2000;
		EXTRACT_BITS(working, data, pos, 4); // 10s of year
		aop_entry.bulletin.year += 10 * working;
		EXTRACT_BITS(working, data, pos, 4); // Units of year
		aop_entry.bulletin.year += working;
		EXTRACT_BITS(working, data, pos, 4); // 100s of day
		day_of_year = 100 * working;
		EXTRACT_BITS(working, data, pos, 4); // 10s of day
		day_of_year += 10 * working;
		EXTRACT_BITS(working, data, pos, 4); // Units of day
		day_of_year += working;
		EXTRACT_BITS(working, data, pos, 4); // 10s of hour
		aop_entry.bulletin.hour = 10 * working;
		EXTRACT_BITS(working, data, pos, 4); // Units of hour
		aop_entry.bulletin.hour += working;
		EXTRACT_BITS(working, data, pos, 4); // 10s of minute
		aop_entry.bulletin.minute = 10 * working;
		EXTRACT_BITS(working, data, pos, 4); // Units of minute
		aop_entry.bulletin.minute += working;
		EXTRACT_BITS(working, data, pos, 4); // 10s of second
		aop_entry.bulletin.second = 10 * working;
		EXTRACT_BITS(working, data, pos, 4); // Units of second
		aop_entry.bulletin.second += working;

		// Compute the actual day of month and month from the day of year
		convert_day_of_year(aop_entry.bulletin.year, day_of_year, aop_entry.bulletin.month, aop_entry.bulletin.day);

		// 86 bits of bulletin
		EXTRACT_BITS(working, data, pos, 19); // Longitude of the ascending node
		aop_entry.ascNodeLongitudeDeg = working / 1000.f;
		EXTRACT_BITS(working, data, pos, 10); // angular separation between two successive ascending nodes
		aop_entry.ascNodeDriftDeg = (working / 1000.f) - 26;
		EXTRACT_BITS(working, data, pos, 14); // nodal period
		aop_entry.orbitPeriodMin = (working / 1000.f) + 95;
		EXTRACT_BITS(working, data, pos, 19); // semi-major axis
		aop_entry.semiMajorAxisKm = (working / 1000.f) + 7000;
		EXTRACT_BITS(working, data, pos, 8); // semi-major axis decay
		aop_entry.semiMajorAxisDriftMeterPerDay = working * -0.1f;
		EXTRACT_BITS(working, data, pos, 16); // inclination
		aop_entry.inclinationDeg = (working / 10000.f) + 97;

		// Mark as out of service until constellation status is parsed
		aop_entry.downlinkStatus = SAT_DNLK_OFF;
		aop_entry.uplinkStatus = SAT_UPLK_OFF;
	}

	static void allcast_packet_decode(const std::string& data, unsigned int &pos, BasePassPredict& pass_predict) {
		uint32_t addressee_identification;
		uint8_t  a_dcs;
		uint8_t  service;

		DEBUG_TRACE("allcast_packet_decode: pos: %u", pos);

		EXTRACT_BITS(addressee_identification, data, pos, 28);
		EXTRACT_BITS(a_dcs, data, pos, 4);
		EXTRACT_BITS(service, data, pos, 8);

		DEBUG_TRACE("allcast_packet_decode: addressee_identification: %08x", addressee_identification);
		DEBUG_TRACE("allcast_packet_decode: a_dcs: %01x", a_dcs);
		DEBUG_TRACE("allcast_packet_decode: service: %02x", service);

		if (service != 0) {
			DEBUG_ERROR("allcast_packet_decode: message service is not allcast (0x00)");
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}

		switch (addressee_identification) {
		case 0xC7: // Constellation Satellite Status - version A
			allcast_constellation_status_decode(data, pos, true, pass_predict);
			break;
		case 0x5F: // Constellation Satellite Status - version B
			allcast_constellation_status_decode(data, pos, false, pass_predict);
			break;
		case 0xBE: // Satellite Orbit Parameters
			DEBUG_TRACE("allcast_packet_decode: Satellite Orbit Parameters: sat=%u", pass_predict.num_records);
			pass_predict.records[pass_predict.num_records].satDcsId = a_dcs;
			allcast_sat_orbit_params_decode(data, pos, pass_predict.records[pass_predict.num_records++]);
			break;
		default:
			DEBUG_ERROR("allcast_packet_decode: unrecognised allcast packet ID (%08x)", addressee_identification);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
			break;
		}

		uint16_t fcs;
		EXTRACT_BITS(fcs, data, pos, 16);
		(void)fcs;
		DEBUG_TRACE("allcast_packet_decode: fcs: %04x", fcs);
	}
public:
	static void decode(const std::string& data, BasePassPredict& pass_predict) {
		unsigned int base_pos = 0;
		pass_predict.num_records = 0;

		while (base_pos < (8 * data.length())) {
			allcast_packet_decode(data, base_pos, pass_predict);
		}
	}
};


class DTEDecoder;

class DTEEncoder {
protected:
	static inline void encode(std::ostringstream& output, const BaseArgosDepthPile& value) {
		if (value == BaseArgosDepthPile::DEPTH_PILE_1)
			output << "1";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_2)
			output << "2";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_3)
			output << "3";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_4)
			output << "4";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_8)
			output << "8";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_12)
			output << "9";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_16)
			output << "10";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_20)
			output << "11";
		else if (value == BaseArgosDepthPile::DEPTH_PILE_24)
			output << "12";
	}
	static inline void encode_frequency(std::ostringstream& output, double& value) {
		unsigned int x = (value * ARGOS_FREQUENCY_MULT) - ARGOS_FREQUENCY_OFFSET;
		std::cout << "freq=" << value << "op=" << x << std::endl;
		encode(output, x);
	}
	static inline void encode(std::ostringstream& output, const BaseArgosMode& value) {
		encode(output, (unsigned int&)value);
	}
	static inline void encode(std::ostringstream& output, const BaseArgosPower& value) {
		encode(output, (unsigned int&)value);
	}
	static inline void encode(std::ostringstream& output, const BaseAqPeriod& value) {
		encode(output, (unsigned int&)value);
	}
	static inline void encode(std::ostringstream& output, const std::time_t& value) {
		output << std::put_time(std::gmtime(&value), "%c");
	}
	static inline void encode(std::ostringstream& output, const unsigned int& value) {
		output << value;
	}
	static inline void encode(std::ostringstream& output, const unsigned int& value, bool hex) {
		std::ios old(nullptr);
		if (hex) {
			old.copyfmt(output);
			output << std::hex << std::uppercase << value;
			output.copyfmt(old);
		} else
			output << value;

	}
	static inline void encode(std::ostringstream& output, const bool& value) {
		encode(output, static_cast<unsigned int>(value));
	}
	static inline void encode(std::ostringstream& output, const int& value) {
		output << value;
	}
	static inline void encode(std::ostringstream& output, const BaseRawData& value) {
		std::string s;
		if (value.length == 0) {
			s = websocketpp::base64_encode(value.str);
		} else {
			s = websocketpp::base64_encode((unsigned char const *)value.ptr, value.length);
		}
		// Don't payload size to exceed max permitted length
		if (s.length() > BASE_MAX_PAYLOAD_LENGTH)
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		output << s;
	}
	static inline void encode(std::ostringstream& output, const double& value) {
		output << value;
	}
	static inline void encode (std::ostringstream& output, const std::string& value) {
		output << value;
	}
	static inline void encode(std::ostringstream& output, const ParamID &value) {
		output << param_map[(int)value].key;
	}

	static void validate(const BaseMap &arg_map, const std::string& value) {
		if (value.length() > BASE_TEXT_MAX_LENGTH) {
			DEBUG_ERROR("parameter %s string length %u is out of bounds", arg_map.name.c_str(), value.length());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
		if (!arg_map.permitted_values.empty() &&
			std::find_if(arg_map.permitted_values.begin(), arg_map.permitted_values.end(), [value](const BaseConstraint x){
				return std::get<std::string>(x) == value;
			}) == arg_map.permitted_values.end()) {
			DEBUG_ERROR("parameter %s not in permitted list", arg_map.name.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static void validate(const BaseMap &arg_map, const double& value) {
		const auto min_value = std::get<double>(arg_map.min_value);
		const auto max_value = std::get<double>(arg_map.max_value);
		if ((min_value != 0 || max_value != 0) &&
			(value < min_value || value > max_value)) {
			DEBUG_ERROR("parameter %s value out of min/max range", arg_map.name.c_str(), value, min_value, max_value);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
		if (!arg_map.permitted_values.empty() &&
			std::find_if(arg_map.permitted_values.begin(), arg_map.permitted_values.end(), [value](const BaseConstraint x){
				return std::get<double>(x) == value;
			}) == arg_map.permitted_values.end()) {
			DEBUG_ERROR("parameter %s not in permitted list", arg_map.name.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static void validate(const BaseMap &arg_map, const unsigned int& value) {
		const auto min_value = std::get<unsigned int>(arg_map.min_value);
		const auto max_value = std::get<unsigned int>(arg_map.max_value);
		if ((min_value != 0 || max_value != 0) &&
			(value < min_value || value > max_value)) {
			DEBUG_ERROR("parameter %s value out of min/max range", arg_map.name.c_str(), value, min_value, max_value);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
		if (!arg_map.permitted_values.empty() &&
			std::find_if(arg_map.permitted_values.begin(), arg_map.permitted_values.end(), [value](const BaseConstraint x){
				return std::get<unsigned int>(x) == value;
			}) == arg_map.permitted_values.end()) {
			DEBUG_ERROR("parameter %s not in permitted list", arg_map.name.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static void validate(const BaseMap &arg_map, const int& value) {
		const auto min_value = std::get<int>(arg_map.min_value);
		const auto max_value = std::get<int>(arg_map.max_value);
		if ((min_value != 0 || max_value != 0) &&
			(value < min_value || value > max_value)) {
			DEBUG_ERROR("parameter %s value out of min/max range", arg_map.name.c_str(), value, min_value, max_value);
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
		if (!arg_map.permitted_values.empty() &&
			std::find_if(arg_map.permitted_values.begin(), arg_map.permitted_values.end(), [value](const BaseConstraint x){
				return std::get<int>(x) == value;
			}) == arg_map.permitted_values.end()) {
			DEBUG_ERROR("parameter %s not in permitted list", arg_map.name.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static void validate(const BaseMap &arg_map, const std::time_t& value) {
		(void)arg_map;
		(void)value;
	}

	static void validate(const BaseMap &arg_map, const BaseRawData& value) {
		(void)arg_map;
		(void)value;
	}

	static void validate(const BaseMap &arg_map, const BaseArgosMode& value) {
		(void)arg_map;
		(void)value;
	}

	static void validate(const BaseMap &arg_map, const BaseArgosPower& value) {
		(void)arg_map;
		(void)value;
	}

	static void validate(const BaseMap &arg_map, const BaseAqPeriod& value) {
		(void)arg_map;
		(void)value;
	}

	static void validate(const BaseMap &arg_map, const BaseArgosDepthPile& value) {
		(void)arg_map;
		(void)value;
	}

public:
	static std::string encode(DTECommand command, ...) {
		unsigned int error_code = 0;
		std::ostringstream buffer;
		std::ostringstream payload;
		unsigned int command_index = (unsigned int)command & RESP_CMD_BASE ?
				((unsigned int)command & ~RESP_CMD_BASE) + (unsigned int)DTECommand::__NUM_REQ : (unsigned int)command;
		const std::string& command_name = command_map[command_index].name;
		const std::vector<BaseMap>& command_args = command_map[command_index].prototype;
		unsigned int expected_args = command_args.size();

		DEBUG_TRACE("command = %u expected_args = %u", (unsigned int)command, expected_args);

		va_list args;
		va_start(args, command);

		if (((unsigned int)command & RESP_CMD_BASE)) {
			error_code = va_arg(args, unsigned int);
			DEBUG_TRACE("error_code %u", error_code);
			if (error_code > 0)
			{
				DEBUG_TRACE("abort error_code %u", error_code);
				encode(payload, error_code);
				expected_args = 0;
			}
		}

		// Ignore additional arguments if an error was indicated or expected number of args does match
		for (unsigned int arg_index = 0; arg_index < expected_args; arg_index++) {

			DEBUG_TRACE("arg_index = %u command = %u encoding = %u", arg_index, (unsigned int)command, (unsigned int)command_args[arg_index].encoding);
			// Add separator for next argument to follow
			if (arg_index > 0)
				payload << ",";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

			switch (command_args[arg_index].encoding) {
				case BaseEncoding::DECIMAL:
					{
						int arg = va_arg(args, int);
						validate(command_args[arg_index], arg);
						encode(payload, arg);
						break;
					}
				case BaseEncoding::HEXADECIMAL:
					{
						unsigned int arg = va_arg(args, unsigned int);
						validate(command_args[arg_index], arg);
						encode(payload, arg, true);
						break;
					}
				case BaseEncoding::BOOLEAN:
					{
						bool arg = va_arg(args, unsigned int);
						encode(payload, arg);
						break;
					}
				case BaseEncoding::BASE64:
					encode(payload, va_arg(args, BaseRawData));
					break;
				case BaseEncoding::ARGOSFREQ:
					{
						double arg = va_arg(args, double);
						validate(command_args[arg_index], arg);
						encode_frequency(payload, arg);
						break;
					}
				case BaseEncoding::TEXT:
					{
						DEBUG_TRACE("Encoding TEXT....");
						std::string arg = va_arg(args, std::string);
						DEBUG_TRACE("Checking %s....", arg.c_str());
						validate(command_args[arg_index], arg);
						encode(payload, arg);
						break;
					}
				default:
					DEBUG_ERROR("parameter type not permitted");
					throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
			}

#pragma GCC diagnostic pop

		}
		va_end(args);

		// Construct header depending on case of request, response with error or response without error
		if ((unsigned int)command & RESP_CMD_BASE) {
			if (error_code)
				buffer << "$N;";
			else
				buffer << "$O;";
		} else {
			buffer << "$";
		}

		// Sanity check payload length
		if (payload.str().size() > BASE_MAX_PAYLOAD_LENGTH)
		{
			DEBUG_ERROR("DTE_PROTOCOL_MESSAGE_TOO_LARGE");
			throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;
		}

		// Append command, separator, payload and terminate
		std::ios old(nullptr);
		old.copyfmt(buffer);
		buffer << command_name << "#" << std::uppercase << std::hex << std::setfill('0') << std::setw(3) << payload.str().size();
		buffer.copyfmt(old);
		buffer << ";" << payload.str() << "\r";

		return buffer.str();
	}

	static std::string encode(DTECommand command, std::vector<ParamID>& params) {
		std::ostringstream buffer;
		std::ostringstream payload;
		const std::string& command_name = command_map[(unsigned int)command & ~RESP_CMD_BASE].name;
		unsigned int expected_args = params.size();

		// Ignore additional arguments if an error was indicated or expected number of args does match
		for (unsigned int arg_index = 0; arg_index < expected_args; arg_index++) {
			// Add separator for next argument to follow
			if (arg_index > 0)
				payload << ",";
			encode(payload, params[arg_index]);
		}

		// Construct header depending on case of request, response with error or response without error
		if ((unsigned int)command & RESP_CMD_BASE) {
			buffer << "$O;";
		} else {
			buffer << "$";
		}

		// Sanity check payload length
		if (payload.str().size() > BASE_MAX_PAYLOAD_LENGTH)
		{
			DEBUG_ERROR("DTE_PROTOCOL_MESSAGE_TOO_LARGE");
			throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;
		}

		// Append command, separator, payload and terminate
		std::ios old(nullptr);
		old.copyfmt(buffer);
		buffer << command_name << "#" << std::uppercase << std::hex << std::setfill('0') << std::setw(3) << payload.str().size();
		buffer.copyfmt(old);
		buffer << ";" << payload.str() << "\r";

		return buffer.str();
	}

	static std::string encode(DTECommand command, std::vector<ParamValue>& param_values) {
		std::ostringstream buffer;
		std::ostringstream payload;
		const std::string& command_name = command_map[(unsigned int)command & ~RESP_CMD_BASE].name;
		unsigned int expected_args = param_values.size();

		// Ignore additional arguments if an error was indicated or expected number of args does match
		for (unsigned int arg_index = 0; arg_index < expected_args; arg_index++) {
			const BaseMap& map = param_map[(unsigned int)param_values[arg_index].param];
			// Add separator for next argument to follow
			if (arg_index > 0)
				payload << ",";
			encode(payload, param_values[arg_index].param);
			payload << "=";
			//std::cout << "arg_index:" << arg_index << " enc: " << (unsigned)param_map[(unsigned int)param_values[arg_index].param].encoding << " type:" << param_values[arg_index].value.index() << "\n";
			if (param_map[(unsigned int)param_values[arg_index].param].encoding == BaseEncoding::HEXADECIMAL) {
				unsigned int value = std::get<unsigned int>(param_values[arg_index].value);
				validate(map, value);
				encode(payload, value, true);
			}
			else if (param_map[(unsigned int)param_values[arg_index].param].encoding == BaseEncoding::ARGOSFREQ) {
				double value = std::get<double>(param_values[arg_index].value);
				validate(map, value);
				encode_frequency(payload, value);
			}
			else
			{
				std::visit([&map, &payload](auto&& arg){validate(map, arg); encode(payload, arg);}, param_values[arg_index].value);
			}
		}

		// Construct header depending on case of request, response with error or response without error
		if ((unsigned int)command & RESP_CMD_BASE) {
			buffer << "$O;";
		} else {
			buffer << "$";
		}

		// Sanity check payload length
		if (payload.str().size() > BASE_MAX_PAYLOAD_LENGTH)
		{
			DEBUG_ERROR("DTE_PROTOCOL_MESSAGE_TOO_LARGE");
			throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;
		}

		// Append command, separator, payload and terminate
		std::ios old(nullptr);
		old.copyfmt(buffer);
		buffer << command_name << "#" << std::uppercase << std::hex << std::setfill('0') << std::setw(3) << payload.str().size();
		buffer.copyfmt(old);
		buffer << ";" << payload.str() << "\r";

		return buffer.str();
	}

	friend DTEDecoder;
};


class DTEDecoder {
private:
	static const DTECommandMap* lookup_command(std::string command_str, bool is_req) {
		unsigned int start = is_req ? 0 : (unsigned int)DTECommand::__NUM_REQ;
		unsigned int end = is_req ? (unsigned int)DTECommand::__NUM_REQ : sizeof(command_map)/sizeof(DTECommandMap);
		for (unsigned int i = start; i < end; i++) {
			if (command_map[i].name == command_str) {
				//std::cout << "command: " << command_str << " match: " << command_map[i].name << "\n";
				return &command_map[i];
			}
		}
		DEBUG_ERROR("DTE_PROTOCOL_UNKNOWN_COMMAND");
		throw DTE_PROTOCOL_UNKNOWN_COMMAND;
	}

	static ParamID lookup_key(std::string key) {
		auto end = sizeof(param_map) / sizeof(BaseMap);
		for (unsigned int i = 0; i < end; i++) {
			if (param_map[i].key == key) {
				return static_cast<ParamID>(i);
			}
		}
		DEBUG_ERROR("DTE_PROTOCOL_PARAM_KEY_UNRECOGNISED, \"%s\"", key.c_str());
		throw DTE_PROTOCOL_PARAM_KEY_UNRECOGNISED;
	}

	static inline std::string fetch_param(std::istringstream& ss) {
		std::string s;
		if (std::getline(ss, s, ','))
			return s;
		DEBUG_ERROR("DTE_PROTOCOL_BAD_FORMAT in %s()", __FUNCTION__);
		throw DTE_PROTOCOL_BAD_FORMAT;
	}

	static double decode_frequency(std::string& s) {
		unsigned int offset_frequency = decode<unsigned int>(s);
		double x = ((double)offset_frequency + ARGOS_FREQUENCY_OFFSET) / ARGOS_FREQUENCY_MULT;
		return x;
	}

	static BaseArgosPower decode_power(std::string& s) {
		if (s == "1") {
			return BaseArgosPower::POWER_3_MW;
		} else if (s == "2") {
			return BaseArgosPower::POWER_40_MW;
		} else if (s == "3") {
			return BaseArgosPower::POWER_200_MW;
		} else if (s == "4") {
			return BaseArgosPower::POWER_500_MW;
		} else {
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%s)", __FUNCTION__, s.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static BaseAqPeriod decode_acquisition_period(std::string& s) {
		if (s == "1") {
			return BaseAqPeriod::AQPERIOD_10_MINS;
		} else if (s == "2") {
			return BaseAqPeriod::AQPERIOD_15_MINS;
		} else if (s == "3") {
			return BaseAqPeriod::AQPERIOD_30_MINS;
		} else if (s == "4") {
			return BaseAqPeriod::AQPERIOD_60_MINS;
		} else if (s == "5") {
			return BaseAqPeriod::AQPERIOD_120_MINS;
		} else if (s == "6") {
			return BaseAqPeriod::AQPERIOD_360_MINS;
		} else if (s == "7") {
			return BaseAqPeriod::AQPERIOD_720_MINS;
		} else if (s == "8") {
			return BaseAqPeriod::AQPERIOD_1440_MINS;
		} else {
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%s)", __FUNCTION__, s.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static BaseArgosPower decode_power(std::istringstream& ss) {
		std::string s = fetch_param(ss);
		return decode_power(s);
	}

	static BaseArgosMode decode_mode(std::string& s) {
		if (s == "0") {
			return BaseArgosMode::OFF;
		} else if (s == "1") {
			return BaseArgosMode::LEGACY;
		} else if (s == "2") {
			return BaseArgosMode::PASS_PREDICTION;
		} else if (s == "3") {
			return BaseArgosMode::DUTY_CYCLE;
		} else {
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%s)", __FUNCTION__, s.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static BaseArgosMode decode_mode(std::istringstream& ss) {
		std::string s = fetch_param(ss);
		return decode_mode(s);
	}

	static BaseArgosDepthPile decode_depth_pile(std::string& s) {
		if (s == "1") {
			return BaseArgosDepthPile::DEPTH_PILE_1;
		} else if (s == "2") {
			return BaseArgosDepthPile::DEPTH_PILE_2;
		} else if (s == "3") {
			return BaseArgosDepthPile::DEPTH_PILE_3;
		} else if (s == "4") {
			return BaseArgosDepthPile::DEPTH_PILE_4;
		} else if (s == "8") {
			return BaseArgosDepthPile::DEPTH_PILE_8;
		} else if (s == "9") {
			return BaseArgosDepthPile::DEPTH_PILE_12;
		} else if (s == "10") {
			return BaseArgosDepthPile::DEPTH_PILE_16;
		} else if (s == "11") {
			return BaseArgosDepthPile::DEPTH_PILE_20;
		} else if (s == "12") {
			return BaseArgosDepthPile::DEPTH_PILE_24;
		} else {
			DEBUG_ERROR("DTE_PROTOCOL_VALUE_OUT_OF_RANGE in %s(%s)", __FUNCTION__, s.c_str());
			throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
		}
	}

	static BaseArgosDepthPile decode_depth_pile(std::istringstream& ss) {
		std::string s = fetch_param(ss);
		return decode_depth_pile(s);
	}

	static std::time_t decode_datestring(std::string& s) {
		std::istringstream ss(s);
		return decode_datestring(ss);
	}

	static std::time_t decode_datestring(std::istringstream& ss) {
		std::tm tm = {};
		if (ss >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y")) {
			return std::mktime(&tm);
		}
		else {
			DEBUG_ERROR("DTE_PROTOCOL_BAD_FORMAT in %s()", __FUNCTION__);
			throw DTE_PROTOCOL_BAD_FORMAT;
		}
	}

	template <typename T>
	static T decode(std::string s, bool is_hex=false)
	{
		std::istringstream ss(s);
		return decode<T>(ss, is_hex);
	}

	template <typename T>
	static T decode(std::istringstream& ss, bool is_hex=false)
	{
		T out;
		if (is_hex)
			ss >> std::hex;
		if (ss >> out)
			return out;
		else {
			DEBUG_ERROR("DTE_PROTOCOL_BAD_FORMAT in %s()", __FUNCTION__);
			throw DTE_PROTOCOL_BAD_FORMAT;
		}
	}

	static std::string decode(std::istringstream& ss) {
		std::string value;
		if (std::getline(ss, value, ','))
			return value;
		else {
			DEBUG_ERROR("DTE_PROTOCOL_BAD_FORMAT in %s()", __FUNCTION__);
			throw DTE_PROTOCOL_BAD_FORMAT;
		}
	}

	static void decode(std::istringstream& ss, std::vector<ParamID>& keys) {
		std::string key;
		while (std::getline(ss, key, ',')) {
			keys.push_back(lookup_key(key));
		}
	}

	static void decode(std::istringstream& ss, std::vector<ParamValue>& key_values) {
		std::string key_str;
		while (std::getline(ss, key_str, ',')) {
			std::smatch base_match;
			std::regex re_key_value("^([A-Z0-9]+)=(.*)$");
			if (std::regex_match(key_str, base_match, re_key_value) && base_match.size() == 3) {
				ParamValue key_value;
				std::string key = base_match.str(1);
				std::string value = base_match.str(2);
				key_value.param = lookup_key(key);
				const BaseMap& param_ref = param_map[(unsigned int)key_value.param];
				switch (param_ref.encoding) {
					case BaseEncoding::DECIMAL:
					{
						int x = decode<int>(value);
						DTEEncoder::validate(param_ref, x);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::HEXADECIMAL:
					{
						unsigned int x = decode<unsigned int>(value, true);
						DTEEncoder::validate(param_ref, x);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::UINT:
					{
						unsigned int x = decode<unsigned int>(value);
						DTEEncoder::validate(param_ref, x);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::BOOLEAN:
					{
						key_value.value = decode<bool>(value);
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::FLOAT:
					{
						double x = decode<double>(value);
						DTEEncoder::validate(param_ref, x);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::TEXT:
					{
						key_value.value = value;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::BASE64:
					{
						key_value.value = websocketpp::base64_decode(value);
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::ARGOSFREQ:
					{
						double x = decode_frequency(value);
						DTEEncoder::validate(param_ref, x);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::ARGOSPOWER:
					{
						BaseArgosPower x = decode_power(value);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::AQPERIOD:
					{
						BaseAqPeriod x = decode_acquisition_period(value);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::DEPTHPILE:
					{
						BaseArgosDepthPile x = decode_depth_pile(value);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::ARGOSMODE:
					{
						BaseArgosMode x = decode_mode(value);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::DATESTRING:
					{
						std::time_t x = decode_datestring(value);
						key_value.value = x;
						key_values.push_back(key_value);
						break;
					}
					case BaseEncoding::KEY_LIST:
					case BaseEncoding::KEY_VALUE_LIST:
					default:
						break;
					}
			}
		}
	}

public:
	static bool decode(const std::string& str, DTECommand& command, unsigned int& error_code, std::vector<BaseType> &arg_list, std::vector<ParamID>& keys, std::vector<ParamValue>& key_values) {
		bool is_req;
		error_code = 0;

		std::smatch base_match;
		std::regex re_command_resp_ok("^\\$O;([A-Z]+)#([0-9a-fA-F]+);(.*)\r$");
		std::regex re_command_resp_nok("^\\$N;([A-Z]+)#([0-9a-fA-F]+);([0-9]+)\r$");
		std::regex re_command_req("^\\$([A-Z]+)#([0-9a-fA-F]+);(.*)\r");

		if (std::regex_match(str, base_match, re_command_resp_ok) && base_match.size() >= 3) {
			is_req = false;
		} else if (std::regex_match(str, base_match, re_command_resp_nok) && base_match.size() == 4 ) {
			is_req = false;
			error_code = decode<unsigned int>(std::string(base_match.str(3)));
		} else if (std::regex_match(str, base_match, re_command_req) && base_match.size() >= 3) {
			is_req = true;
		} else {
			//std::cout << "No matching regexp\n";
			return false;
		}

		// This will throw an exception if the command is not found
		const DTECommandMap *cmd_ref = lookup_command(std::string(base_match.str(1)), is_req);
		command = cmd_ref->command;

		//std::cout << "command=" << (unsigned)command << " regexp=" << base_match.size() << "\n";
		//std::cout << "required_args=" << cmd_ref->prototype.size() << "\n";

		if (base_match.size() == 4 && error_code == 0) {

			unsigned int payload_size = decode<unsigned int>(std::string(base_match.str(2)), true);
			//std::cout << "payload_size=" << payload_size << "\n";
			std::string payload = std::string(base_match.str(3));

			if (payload_size != payload.length()) {
				DEBUG_ERROR("DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH, expected %ld but got %ld", payload_size, payload.length());
				throw DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH;
			}

			if (payload_size > BASE_MAX_PAYLOAD_LENGTH) {
				DEBUG_ERROR("DTE_PROTOCOL_MESSAGE_TOO_LARGE");
				throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;
			}

			// KEY_LIST is permitted to be zero length
			if (cmd_ref->prototype.size() && !payload_size &&
				cmd_ref->prototype[0].encoding != BaseEncoding::KEY_LIST) {
				DEBUG_ERROR("DTE_PROTOCOL_MISSING_ARG");
				throw DTE_PROTOCOL_MISSING_ARG;
			}

			if (payload_size && !cmd_ref->prototype.size()) {
				DEBUG_ERROR("DTE_PROTOCOL_UNEXPECTED_ARG");
				throw DTE_PROTOCOL_UNEXPECTED_ARG;
			}

			std::istringstream ss(payload);

			// Iterate over expected parameters based on the command map entries
			for (unsigned int arg_index = 0; arg_index < cmd_ref->prototype.size(); arg_index++) {
				if (arg_index > 0) {
					// Skip over parameter separator and check it is a "," character
					unsigned char x;
					ss >> x;
					if (x == std::char_traits<char>::eof()) {
						DEBUG_ERROR("DTE_PROTOCOL_MISSING_ARG (EOF");
						throw DTE_PROTOCOL_MISSING_ARG;
					}
					if (x != ',') {
						DEBUG_ERROR("DTE_PROTOCOL_BAD_FORMAT in %s()", __FUNCTION__);
						throw DTE_PROTOCOL_BAD_FORMAT;
					}
				}
				switch (cmd_ref->prototype[arg_index].encoding) {
				case BaseEncoding::KEY_VALUE_LIST:
					DEBUG_TRACE("BaseEncoding::KEY_VALUE_LIST");
					decode(ss, key_values);
					break;
				case BaseEncoding::KEY_LIST:
					DEBUG_TRACE("BaseEncoding::KEY_LIST");
					decode(ss, keys);
					break;
				case BaseEncoding::DECIMAL:
				{
					int val = decode<int>(ss);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::HEXADECIMAL:
				{
					DEBUG_TRACE("BaseEncoding::HEXADECIMAL");
					unsigned int val = decode<unsigned int>(ss, true);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::UINT:
				{
					DEBUG_TRACE("BaseEncoding::UINT");
					unsigned int val = decode<unsigned int>(ss);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::BOOLEAN:
				{
					DEBUG_TRACE("BaseEncoding::BOOLEAN");
					bool val = decode<bool>(ss);
					arg_list.push_back(val);
					break;
				}
				break;
				case BaseEncoding::FLOAT:
				{
					DEBUG_TRACE("BaseEncoding::FLOAT");
					double val = decode<double>(ss);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::TEXT:
				{
					DEBUG_TRACE("BaseEncoding::TEXT");
					std::string val = decode(ss);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::BASE64:
				{
					DEBUG_TRACE("BaseEncoding::BASE64");
					arg_list.push_back(websocketpp::base64_decode(decode(ss)));
					break;
				}
				case BaseEncoding::DATESTRING:
				case BaseEncoding::DEPTHPILE:
				case BaseEncoding::ARGOSMODE:
				case BaseEncoding::ARGOSPOWER:
				case BaseEncoding::AQPERIOD:
				case BaseEncoding::ARGOSFREQ:
				default:
					DEBUG_ERROR("BaseEncoding::Not supported");
					break;
				}
			}
		}

		return true;
	}
};

