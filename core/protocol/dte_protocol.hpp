#include <ios>
#include <iomanip>
#include <ctime>
#include <stdarg.h>
#include <sstream>

#include "debug.hpp"

#include "base64.hpp"

#include "dte_params.hpp"
#include "dte_commands.hpp"

class DTEEncoder {
private:
	static inline void encode(std::ostringstream& output, ParamArgosDepthPile value) {
	}
	static inline void encode(std::ostringstream& output, ParamArgosMode value) {
		encode(output, (unsigned int&)value);
	}
	static inline void encode(std::ostringstream& output, ParamArgosPower value) {
		if (value == ParamArgosPower::POWER_250_MW)
			output << "250";
		else if (value == ParamArgosPower::POWER_500_MW)
			output << "500";
		else if (value == ParamArgosPower::POWER_750_MW)
			output << "750";
		else if (value == ParamArgosPower::POWER_1000_MW)
			output << "1000";
	}
	static inline void encode(std::ostringstream& output, const std::time_t value) {
		output << std::asctime(std::gmtime(&value));
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
	static inline void encode(std::ostringstream& output, const DTEBase64& value) {
		output << websocketpp::base64_encode((unsigned char const *)value.ptr, value.length);
	}
	static inline void encode(std::ostringstream& output, const float& value) {
		output << value;
	}
	static inline void encode (std::ostringstream& output, const std::string& value) {
		output << value;
	}
	static inline void encode(std::ostringstream& output, const ParamID value) {
		output << param_map[(int)value].key;
	}

public:
	static std::string encode(DTECommand command, ...) {
		bool error_handled = false;
		bool is_key_list = false;
		bool is_key_value_list = false;
		unsigned int error_code = 0;
		unsigned int length = 0;
		unsigned int arg_index = 0;
		std::ostringstream buffer;
		std::ostringstream payload;
		const std::string& command_name = command_map[(unsigned int)command].name;
		const std::vector<DTECommandArgMap>& command_args = command_map[(unsigned int)command].prototype;
		unsigned int expected_args = command_args.size();

		va_list args;
		va_start(args, command);

		if (((unsigned int)command & RESP_CMD_BASE)) {
			error_code = va_arg(args, unsigned int);
			if (error_code > 0)
				encode(payload, error_code);
		}

		// Ignore additional arguments if an error was indicated or expected number of args does match
		while (error_code == 0 && expected_args >= (arg_index + 1)) {

			while (1) {
				// If we are in key list or key/value list mode then we treat the
				// input parameters as ParamID or ParamValue
				if (is_key_list) {
					ParamID param = va_arg(args, ParamID);
					if (param == ParamID::__NULL_PARAM) {
						expected_args = 0;
					} else {

						// Add separator for next argument to follow
						if (arg_index > 0)
							payload << ",";

						encode(payload, param);
					}
				} else if (is_key_value_list) {
					ParamValue param_value = va_arg(args, ParamValue);
					if (param_value.param == ParamID::__NULL_PARAM) {
						expected_args = 0;
					} else {
						// Add separator for next argument to follow
						if (arg_index > 0)
							payload << ",";

						encode(payload, param_value.param);
						payload << "=";
						if (param_map[(unsigned int)param_value.param].encoding == ParamEncoding::HEXADECIMAL)
							encode(payload, std::get<unsigned int>(param_value.value), true);
						else
							std::visit([&payload](auto&& arg){encode(payload, arg);}, param_value.value);
					}
				} else {
					// Add separator for next argument to follow
					if (arg_index > 0)
						payload << ",";

					// We are not in key list or key/value list mode
					switch (command_args[arg_index].type) {
					case DTECommandArgType::KEY_LIST:
						is_key_list = true;
						expected_args = 0xFFFF;
						continue;
					case DTECommandArgType::KEY_VALUE_LIST:
						is_key_value_list = true;
						expected_args = 0xFFFF;
						continue;
					case DTECommandArgType::DECIMAL:
						encode(payload, va_arg(args, int));
						break;
					case DTECommandArgType::HEXADECIMAL:
						encode(payload, va_arg(args, unsigned int), true);
						break;
					case DTECommandArgType::BASE64:
						encode(payload, va_arg(args, DTEBase64));
						break;
					case DTECommandArgType::TEXT:
						encode(payload, va_arg(args, std::string));
						break;
					default:
						break;
					}
				}
				break;
			}

			arg_index++;
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

		// Append command, separator, payload and terminate
		std::ios old(nullptr);
		old.copyfmt(buffer);
		buffer << command_name << "#" << std::uppercase << std::hex << std::setfill('0') << std::setw(3) << payload.str().size();
		buffer.copyfmt(old);
		buffer << ";" << payload.str() << "\r";

		return buffer.str();
	}
};
