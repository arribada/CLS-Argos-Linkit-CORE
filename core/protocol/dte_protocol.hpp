#include <ios>
#include <iomanip>
#include <ctime>
#include <stdarg.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>
#include <regex>

#include "debug.hpp"

#include "base64.hpp"

#include "dte_params.hpp"
#include "dte_commands.hpp"
#include "error.hpp"


class DTEDecoder;

class DTEEncoder {
protected:
	static inline void encode(std::ostringstream& output, const BaseArgosDepthPile& value) {
		encode(output, (unsigned int&)value);
	}
	static inline void encode(std::ostringstream& output, const BaseArgosMode& value) {
		encode(output, (unsigned int&)value);
	}
	static inline void encode(std::ostringstream& output, const BaseArgosPower& value) {
		if (value == BaseArgosPower::POWER_250_MW)
			output << "250";
		else if (value == BaseArgosPower::POWER_500_MW)
			output << "500";
		else if (value == BaseArgosPower::POWER_750_MW)
			output << "750";
		else if (value == BaseArgosPower::POWER_1000_MW)
			output << "1000";
	}
	static inline void encode(std::ostringstream& output, const std::time_t& value) {
		output << std::put_time(std::gmtime(&value), "%c");
	}
	static inline void encode(std::ostringstream& output, const unsigned int& value) {
		output << value;
	}
	static inline void encode(std::ostringstream& output, const unsigned int& value, bool hex) {
		std::ios old(nullptr);
		old.copyfmt(output);
		output << std::hex << std::uppercase << value;
		output.copyfmt(old);
	}
	static inline void encode(std::ostringstream& output, const bool& value) {
		encode(output, (const unsigned int)value);
	}
	static inline void encode(std::ostringstream& output, const int& value) {
		output << value;
	}
	static inline void encode(std::ostringstream& output, const BaseRawData& value) {
		std::string s = websocketpp::base64_encode((unsigned char const *)value.ptr, value.length);
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
	}

	static void validate(const BaseMap &arg_map, const BaseRawData& value) {
	}

	static void validate(const BaseMap &arg_map, const BaseArgosMode& value) {
	}

	static void validate(const BaseMap &arg_map, const BaseArgosPower& value) {
	}

	static void validate(const BaseMap &arg_map, const BaseArgosDepthPile& value) {
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

			switch (command_args[arg_index].encoding) {
			case BaseEncoding::DECIMAL:
			{
				int arg = va_arg(args, int);
				validate(command_args[arg_index], arg);
				encode(payload, arg);
			}
			break;
			case BaseEncoding::HEXADECIMAL:
			{
				unsigned int arg = va_arg(args, unsigned int);
				validate(command_args[arg_index], arg);
				encode(payload, arg, true);
			}
			break;
			case BaseEncoding::BASE64:
				encode(payload, va_arg(args, BaseRawData));
				break;
			case BaseEncoding::TEXT:
			{
				DEBUG_TRACE("Encoding TEXT....");
				std::string arg = va_arg(args, std::string);
				DEBUG_TRACE("Checking %s....", arg.c_str());
				validate(command_args[arg_index], arg);
				encode(payload, arg);
			}
			break;
			default:
				DEBUG_ERROR("parameter type not permitted");
				throw DTE_PROTOCOL_VALUE_OUT_OF_RANGE;
				break;
			}
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
			throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;

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
			throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;

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
			if (param_map[(unsigned int)param_values[arg_index].param].encoding == BaseEncoding::HEXADECIMAL) {
				unsigned int value = std::get<unsigned int>(param_values[arg_index].value);
				validate(map, value);
				encode(payload, value, true);
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
			throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;

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
		throw DTE_PROTOCOL_UNKNOWN_COMMAND;
	}

	static ParamID lookup_key(std::string key) {
		auto end = sizeof(param_map) / sizeof(BaseMap);
		for (unsigned int i = 0; i < end; i++) {
			if (param_map[i].key == key) {
				return static_cast<ParamID>(i);
			}
		}
		throw DTE_PROTOCOL_PARAM_KEY_UNRECOGNISED;
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
		ss >> out;
		return out;
	}

	static void decode(std::istringstream& ss, std::vector<ParamID>& keys) {
		std::string key;
		while (std::getline(ss, key, ',')) {
			keys.push_back(lookup_key(key));
		}
	}

	static void decode(std::istringstream& ss, std::vector<ParamValue>& key_values) {
		// TODO
	}

public:
	static bool decode(std::string& str, DTECommand& command, unsigned int& error_code, std::vector<BaseType> &arg_list, std::vector<ParamID>& keys, std::vector<ParamValue>& key_values) {
		bool is_req = false;
		bool is_valid = false;

		std::smatch base_match;
		std::regex re_command_resp_ok("^\\$O;([A-Z]+)#([0-9a-fA-F]+);(.*)\r$");
		std::regex re_command_resp_nok("^\\$N;([A-Z]+)#([0-9a-fA-F]+);([0-9]+)\r$");
		std::regex re_command_req("^\\$([A-Z]+)#([0-9a-fA-F]+);(.*)\r");

		if (std::regex_match(str, base_match, re_command_resp_ok) && base_match.size() >= 3) {
			is_valid = true;
			error_code = 0;
		} else if (std::regex_match(str, base_match, re_command_resp_nok) && base_match.size() == 4 ) {
			is_valid = true;
			error_code = decode<unsigned int>(std::string(base_match.str(3)));
		} else if (std::regex_match(str, base_match, re_command_req) && base_match.size() >= 3) {
			is_valid = true;
			is_req = true;
			error_code = 0;
		}

		// Do not proceed without a valid command match e.g., only a partial buffer was presented
		if (!is_valid)
			return false;

		// This will throw an exception if the command is not found
		const DTECommandMap *cmd_ref = lookup_command(std::string(base_match.str(1)), is_req);
		command = cmd_ref->command;

		if (base_match.size() == 4 && error_code == 0) {

			std::string payload = std::string(base_match.str(3));
			unsigned int payload_size = decode<unsigned int>(std::string(base_match.str(2)), true);
			std::istringstream ss(payload);

			//std::cout << "payload=" << payload << " size=" << payload_size << "\n";
			//std::cout << "args=" << cmd_ref->prototype.size() << "\n";

			if (payload_size != payload.length()) {
				//std::cout << "DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH\n";
				throw DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH;
			}

			if (payload_size > BASE_MAX_PAYLOAD_LENGTH) {
				//std::cout << "DTE_PROTOCOL_MESSAGE_TOO_LARGE\n";
				throw DTE_PROTOCOL_MESSAGE_TOO_LARGE;
			}

			if (cmd_ref->prototype.size() && !payload_size) {
				//std::cout << "DTE_PROTOCOL_MISSING_ARG\n";
				throw DTE_PROTOCOL_MISSING_ARG;
			}

			if (payload_size && !cmd_ref->prototype.size()) {
				//std::cout << "DTE_PROTOCOL_UNEXPECTED_ARG\n";
				throw DTE_PROTOCOL_UNEXPECTED_ARG;
			}

			// Iterate over expected parameters based on the command map entries
			for (unsigned int arg_index = 0; arg_index < cmd_ref->prototype.size(); arg_index++) {
				if (arg_index > 0) {
					// Skip over parameter separator and check it is a "," character
					unsigned char x;
					ss >> x;
					if (x == std::char_traits<char>::eof()) {
						//std::cout << "DTE_PROTOCOL_MISSING_ARG (EOF)\n";
						throw DTE_PROTOCOL_MISSING_ARG;
					}
					if (x != ',') {
						//std::cout << "DTE_PROTOCOL_BAD_FORMAT\n";
						throw DTE_PROTOCOL_BAD_FORMAT;
					}
				}
				switch (cmd_ref->prototype[arg_index].encoding) {
				case BaseEncoding::KEY_VALUE_LIST:
					decode(ss, key_values);
					break;
				case BaseEncoding::KEY_LIST:
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
					unsigned int val = decode<unsigned int>(ss, true);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::UINT:
				{
					unsigned int val = decode<unsigned int>(ss);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				case BaseEncoding::BOOLEAN:
				{
					bool val = decode<bool>(ss);
					arg_list.push_back(val);
					break;
				}
				break;
				case BaseEncoding::FLOAT:
				{
					double val = decode<double>(ss);
					DTEEncoder::validate(cmd_ref->prototype[arg_index], val);
					arg_list.push_back(val);
					break;
				}
				default:
					break;
				}
			}
		}

		return is_valid;
	}
};

