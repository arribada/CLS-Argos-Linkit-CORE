#include <algorithm>

#include "dte_protocol.hpp"
#include "config_store.hpp"
#include "memory_access.hpp"

using namespace std::literals::string_literals;


// This governs the maximum number of log entries we can read out in a single request
#define DTE_HANDLER_MAX_LOG_DUMP_ENTRIES          16U

enum class DTEAction {
	NONE,    // Default action is none
	AGAIN,   // More data is remaining in a multi-message response, call the handler again with the same input message
	RESET,   // Deferred action since DTE must respond first before reset can be performed
	FACTR,   // Deferred action since DTE must respond first before factory reset can be performed
	SECUR    // DTE service must be notified when SECUR is requested to grant privileges for OTA FW commands
};

enum class DTEError {
	OK,
	INCORRECT_COMMAND,
	NO_LENGTH_DELIMITER,
	NO_DATA_DELIMITER,
	DATA_LENGTH_MISMATCH,
	INCORRECT_DATA
};

// The DTEHandler requires access to the following system objects that are extern declared
extern ConfigurationStore *configuration_store;
extern MemoryAccess *memory_access;
extern Logger *sensor_log;
extern Logger *system_log;

class DTEHandler {
private:
	unsigned int m_dumpd_count;
	unsigned int m_dumpd_offset;

public:
	DTEHandler() {
		m_dumpd_count = 0;
		m_dumpd_offset = 0;
	}

	static std::string PARML_REQ(int error_code) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARML_RESP, error_code);
		}

		// Build up a list of all implemented parameters
		std::vector<ParamID> params;
		for (unsigned int i = 0; i < sizeof(param_map)/sizeof(BaseMap); i++) {
			if (param_map[i].is_implemented) {
				params.push_back(static_cast<ParamID>(i));
			}
		}

		return DTEEncoder::encode(DTECommand::PARML_RESP, params);
	}

	static std::string PARMW_REQ(int error_code, std::vector<ParamValue>& param_values) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARML_RESP, error_code);
		}

		for(unsigned int i = 0; i < param_values.size(); i++)
			configuration_store->write_param(param_values[i].param, param_values[i].value);

		return DTEEncoder::encode(DTECommand::PARMW_RESP, DTEError::OK);
	}

	static std::string PARMR_REQ(int error_code, std::vector<ParamID>& params) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARMR_RESP, error_code);
		}

		// Check special case where params is zero length => retrieve all parameter key types
		if (params.size() == 0) {
			// Extract all parameter keys
			for (unsigned int i = 0; i < sizeof(param_map)/sizeof(BaseMap); i++) {
				if (param_map[i].is_implemented &&
					param_map[i].key[2] == 'P')
					params.push_back((ParamID)i);
			}
		}

		std::vector<ParamValue> param_values;
		for (unsigned int i = 0; i < params.size(); i++) {
			BaseType x = configuration_store->read_param<BaseType>(params[i]);
			ParamValue p = {
				params[i],
				x
			};
			param_values.push_back(p);
		}

		return DTEEncoder::encode(DTECommand::PARMR_RESP, param_values);
	}

	static std::string STATR_REQ(int error_code, std::vector<ParamID>& params) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::STATR_RESP, error_code);
		}

		// Check special case where params is zero length => retrieve all technical key types
		if (params.size() == 0) {
			// Extract all parameter keys
			for (unsigned int i = 0; i < sizeof(param_map)/sizeof(BaseMap); i++) {
				if (param_map[i].is_implemented &&
					param_map[i].key[2] == 'T')
					params.push_back((ParamID)i);
			}
		}

		std::vector<ParamValue> param_values;
		for (unsigned int i = 0; i < params.size(); i++) {
			BaseType x = configuration_store->read_param<BaseType>(params[i]);
			ParamValue p = {
				params[i],
				x
			};
			param_values.push_back(p);
		}

		return DTEEncoder::encode(DTECommand::STATR_RESP, param_values);
	}

	static std::string PROFW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (!error_code) {
			configuration_store->write_param(ParamID::PROFILE_NAME, arg_list[0]);
		}

		return DTEEncoder::encode(DTECommand::PROFW_RESP, error_code);
	}

	static std::string PROFR_REQ(int error_code) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PROFR_RESP, error_code);
		}

		return DTEEncoder::encode(DTECommand::PROFR_RESP, error_code, configuration_store->read_param<std::string>(ParamID::PROFILE_NAME));
	}

	static std::string SECUR_REQ(int error_code, std::vector<BaseType>& arg_list) {

		// TODO: the accesscode parameter is presently ignored
		(void)arg_list;

		return DTEEncoder::encode(DTECommand::SECUR_RESP, error_code);

	}

	static std::string RSTBW_REQ(int error_code) {

		return DTEEncoder::encode(DTECommand::RSTBW_RESP, error_code);

	}

	static std::string FACTW_REQ(int error_code) {

		return DTEEncoder::encode(DTECommand::FACTW_RESP, error_code);

	}

	static std::string DUMPM_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::DUMPM_RESP, error_code);
		}

		unsigned int address = std::get<unsigned int>(arg_list[0]);
		unsigned int length = std::get<unsigned int>(arg_list[1]);
		BaseRawData raw = {
			.ptr = memory_access->get_physical_address(address, length),
			.length = length,
			.str = ""
		};

		return DTEEncoder::encode(DTECommand::DUMPM_RESP, error_code, raw);
	}

	static std::string ZONEW_REQ(int error_code, std::vector<BaseType>& arg_list) {

#ifdef OLD_ZONE_FORMAT_WORKAROUND
		// The zone file format used on the phone app is of an older version so lets just ignore it and say we succeeded
		return "$O;ZONEW#000;\r";
#else
		if (!error_code) {
			BaseZone zone;
			std::string zone_bits = std::get<std::string>(arg_list[0]);
			ZoneCodec::decode(zone_bits, zone);
			configuration_store->write_zone(zone);
		}

		return DTEEncoder::encode(DTECommand::ZONEW_RESP, error_code);
#endif
	}

	static std::string ZONER_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::ZONER_RESP, error_code);
		}

		unsigned int zone_id = std::get<unsigned int>(arg_list[0]);
		BaseZone& zone = configuration_store->read_zone((uint8_t)zone_id);

		BaseRawData zone_raw;
		zone_raw.length = 0;
		ZoneCodec::encode(zone, zone_raw.str);

		return DTEEncoder::encode(DTECommand::ZONER_RESP, error_code, zone_raw);
	}

	static std::string PASPW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (!error_code) {
			BasePassPredict pass_predict;
			std::string paspw_bits = std::get<std::string>(arg_list[0]);
			PassPredictCodec::decode(paspw_bits, pass_predict);
			configuration_store->write_pass_predict(pass_predict);
		}

		return DTEEncoder::encode(DTECommand::PASPW_RESP, error_code);
	}

	std::string DUMPD_REQ(int error_code, std::vector<BaseType>& arg_list, DTEAction& action) {

		action = DTEAction::NONE;

		if (error_code) {
			// Reset state variables back to zero if an error arises
			m_dumpd_count = 0;
			m_dumpd_offset = 0;
			return DTEEncoder::encode(DTECommand::DUMPD_RESP, error_code);
		}

		Logger *logger;

		// Extract the d_type parameter from arg_list to determine which log file to use
		unsigned int d_type = std::get<unsigned int>(arg_list[0]);
		if ((unsigned int)BaseLogDType::INTERNAL == d_type)
		{
			logger = system_log;
		} else {
			logger = sensor_log;
		}

		// Check to see if this is the first item
		unsigned int total_entries = logger->num_entries();
		if (0 == m_dumpd_count) {
			m_dumpd_count = (total_entries + (DTE_HANDLER_MAX_LOG_DUMP_ENTRIES-1)) / DTE_HANDLER_MAX_LOG_DUMP_ENTRIES;
			m_dumpd_offset = 0;
		}

		LogEntry log_entries[DTE_HANDLER_MAX_LOG_DUMP_ENTRIES];
		BaseRawData raw_data;
		unsigned int start_index = m_dumpd_offset * DTE_HANDLER_MAX_LOG_DUMP_ENTRIES;
		unsigned int num_entries = total_entries - start_index;
		raw_data.ptr = log_entries;
		raw_data.length = std::min(DTE_HANDLER_MAX_LOG_DUMP_ENTRIES, num_entries);

		for (unsigned int i = 0; i < raw_data.length; i++)
			logger->read(&log_entries[i], i + start_index);

		raw_data.length *= sizeof(LogEntry);

		std::string msg = DTEEncoder::encode(DTECommand::DUMPD_RESP, error_code, m_dumpd_offset, m_dumpd_count, raw_data);

		m_dumpd_offset++; // Increment in readiness for next iteration
		if (m_dumpd_offset == m_dumpd_count) {
			m_dumpd_count = 0; // Marks the sequence as complete
		} else {
			action = DTEAction::AGAIN;  // Inform caller that we need another iteration
		}

		return msg;
	}

public:
	DTEAction handle_dte_message(const std::string& req, std::string& resp) {
		DTECommand command;
		std::vector<ParamID> params;
		std::vector<ParamValue> param_values;
		std::vector<BaseType> arg_list;
		unsigned int error_code = (unsigned int)DTEError::OK;
		DTEAction action = DTEAction::NONE;

		try {
			if (!DTEDecoder::decode(req, command, error_code, arg_list, params, param_values))
				return action;
		} catch (ErrorCode e) {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

			switch (e) {
			case ErrorCode::DTE_PROTOCOL_MESSAGE_TOO_LARGE:
			case ErrorCode::DTE_PROTOCOL_PARAM_KEY_UNRECOGNISED:
			case ErrorCode::DTE_PROTOCOL_UNEXPECTED_ARG:
			case ErrorCode::DTE_PROTOCOL_VALUE_OUT_OF_RANGE:
			case ErrorCode::DTE_PROTOCOL_MISSING_ARG:
			case ErrorCode::DTE_PROTOCOL_BAD_FORMAT:
				error_code = (unsigned int)DTEError::INCORRECT_DATA;
				break;
			case ErrorCode::DTE_PROTOCOL_PAYLOAD_LENGTH_MISMATCH:
				error_code = (unsigned int)DTEError::DATA_LENGTH_MISMATCH;
				break;
			case ErrorCode::DTE_PROTOCOL_UNKNOWN_COMMAND:
				error_code = (unsigned int)DTEError::INCORRECT_COMMAND;
				break;
			default:
				throw e; // Error code is unexpected
				break;
			}
		}

		switch(command) {
		case DTECommand::PARML_REQ:
			resp = PARML_REQ(error_code);
			break;
		case DTECommand::PARMW_REQ:
			resp = PARMW_REQ(error_code, param_values);
			break;
		case DTECommand::PARMR_REQ:
			resp = PARMR_REQ(error_code, params);
			break;
		case DTECommand::STATR_REQ:
			resp = STATR_REQ(error_code, params);
			break;
		case DTECommand::PROFW_REQ:
			resp = PROFW_REQ(error_code, arg_list);
			break;
		case DTECommand::PROFR_REQ:
			resp = PROFR_REQ(error_code);
			break;
		case DTECommand::SECUR_REQ:
			resp = SECUR_REQ(error_code, arg_list);
			if (!error_code) action = DTEAction::SECUR;
			break;
		case DTECommand::RSTBW_REQ:
			resp = RSTBW_REQ(error_code);
			if (!error_code) action = DTEAction::RESET;
			break;
		case DTECommand::FACTW_REQ:
			resp = FACTW_REQ(error_code);
			if (!error_code) action = DTEAction::FACTR;
			break;
		case DTECommand::DUMPM_REQ:
			resp = DUMPM_REQ(error_code, arg_list);
			break;
		case DTECommand::ZONEW_REQ:
			resp = ZONEW_REQ(error_code, arg_list);
			break;
		case DTECommand::ZONER_REQ:
			resp = ZONER_REQ(error_code, arg_list);
			break;
		case DTECommand::PASPW_REQ:
			resp = PASPW_REQ(error_code, arg_list);
			break;
		case DTECommand::DUMPD_REQ:
			resp = DUMPD_REQ(error_code, arg_list, action);
			break;
		default:
			break;
		}

		return action;
	}

#pragma GCC diagnostic pop

};
