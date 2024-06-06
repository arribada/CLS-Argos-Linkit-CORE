#pragma once

#include <algorithm>
#include <string>
#include <map>

#include "dte_protocol.hpp"
#include "config_store.hpp"
#include "memory_access.hpp"
#include "timeutils.hpp"
#include "calibration.hpp"
#include "artic_device.hpp"

using namespace std::literals::string_literals;


// This governs the maximum number of log entries we can read out in a single request
#define DTE_HANDLER_MAX_LOG_DUMP_ENTRIES          8U

enum class DTEAction {
	NONE,    // Default action is none
	AGAIN,   // More data is remaining in a multi-message response, call the handler again with the same input message
	RESET,   // Deferred action since DTE must respond first before reset can be performed
	FACTR,   // Deferred action since DTE must respond first before factory reset can be performed
	SECUR,   // DTE service must be notified when SECUR is requested to grant privileges for OTA FW commands
	CONFIG_UPDATED,  // Notified on a successful PARMW
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
extern ArticDevice *artic_device;

class DTEHandler : public ArticEventListener {
private:
	// Tables
	static inline std::map<unsigned int, std::string> m_logger_dump = {
		{0, "system.log"},
		{1, "sensor.log"},
		{2, "ALS"},
		{3, "PH"},
		{4, "RTD"},
		{5, "CDT"},
		{6, "AXL"},
		{7, "PRESSURE"},
		{8, "CAM"},
	};
	static inline std::map<unsigned int, std::string> m_logger_erase = {
		{1, "sensor.log"},
		{2, "system.log"},
		{4, "ALS"},
		{5, "PH"},
		{6, "RTD"},
		{7, "CDT"},
		{8, "AXL"},
		{9, "PRESSURE"},
		{10, "CAM"},
	};
	static inline std::map<unsigned int, std::string> m_scalx = {
		{0, "AXL"},
		{1, "PRS"},
		{2, "ALS"},
		{3, "PH"},
		{4, "RTD"},
		{5, "CDT"},
		{6, "MCP47X6"},
	};
	unsigned int m_dumpd_NNN;
	unsigned int m_dumpd_mmm;
	bool m_artic_device_active;

public:
	DTEHandler() {
		m_dumpd_NNN = 0;
		m_dumpd_mmm = 0;
		m_artic_device_active = false;
	}
	virtual ~DTEHandler() {}

	static std::string PARML_REQ(int error_code) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARML_RESP, error_code);
		}

		// Build up a list of all implemented parameters
		std::vector<ParamID> params;
		for (unsigned int i = 0; i < param_map_size; i++) {
			if (param_map[i].is_implemented) {
				params.push_back(static_cast<ParamID>(i));
			}
		}

		return DTEEncoder::encode(DTECommand::PARML_RESP, params);
	}

	static std::string PARMW_REQ(int error_code, std::vector<ParamValue>& param_values, DTEAction& action) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARMW_RESP, error_code);
		}

		for (unsigned int i = 0; i < param_values.size(); i++) {
			if (param_map[(int)param_values[i].param].is_writable)
				configuration_store->write_param(param_values[i].param, param_values[i].value);
			else
				DEBUG_WARN("DTEHandler::PARMW_REQ: not writing read-only attribute %s", param_map[(int)param_values[i].param].name.c_str());
		}

		// Save all the parameters
		configuration_store->save_params();

		// Notify configuration updated action
		action = DTEAction::CONFIG_UPDATED;

		return DTEEncoder::encode(DTECommand::PARMW_RESP, DTEError::OK);
	}

	static std::string PARMR_REQ(int error_code, std::vector<ParamID>& params) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::PARMR_RESP, error_code);
		}

		// Check special case where params is zero length => retrieve all parameter key types
		if (params.size() == 0) {
			// Extract all parameter keys
			for (unsigned int i = 0; i < param_map_size; i++) {
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
			for (unsigned int i = 0; i < param_map_size; i++) {
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

	static std::string RSTVW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::RSTVW_RESP, error_code);
		}

		unsigned int variable_id = std::get<unsigned int>(arg_list[0]);
		unsigned int zero = 0;

		if (variable_id == 1) {
			// TX_COUNTER
			configuration_store->write_param(ParamID::TX_COUNTER, zero);
			configuration_store->save_params();
		} else if (variable_id == 3) {
			// RX_COUNTER
			configuration_store->write_param(ParamID::ARGOS_RX_COUNTER, zero);
			configuration_store->save_params();
		} else if (variable_id == 4) {
			// RX_TIME
			configuration_store->write_param(ParamID::ARGOS_RX_TIME, zero);
			configuration_store->save_params();
		} else {
			// Invalid variable ID
			error_code = (int)DTEError::INCORRECT_DATA;
		}

		return DTEEncoder::encode(DTECommand::RSTVW_RESP, error_code);
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

	static std::string PASPW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		while (!error_code) {
			BasePassPredict pass_predict;
			std::string paspw_bits = std::get<std::string>(arg_list[0]);
			try {
				PassPredictCodec::decode(paspw_bits, pass_predict);
			} catch (ErrorCode e) {
				error_code = (int)DTEError::INCORRECT_DATA;
				break;  // Do not write configuration store
			}

			// If the number of records is zero then return an incorrect data error and flag this
			// back to the user without updating the configuration store
			if (pass_predict.num_records == 0) {
				DEBUG_ERROR("DTEHandler::PASPW_REQ: no AOP records, so not updating the config store");
				error_code = (int)DTEError::INCORRECT_DATA;
				break;
			}

			// Scan through all the records and find the entry whose bulletin date
			// is the most recent, skipping any non-operational satellites
			std::time_t argos_aop_date = 0;
			for (unsigned int i = 0; i < pass_predict.num_records; i++)
			{
				if (pass_predict.records[i].bulletin.year) {
					std::time_t t = convert_epochtime(pass_predict.records[i].bulletin.year, pass_predict.records[i].bulletin.month, pass_predict.records[i].bulletin.day, pass_predict.records[i].bulletin.hour, pass_predict.records[i].bulletin.minute, pass_predict.records[i].bulletin.second);
					if (t > argos_aop_date)
						argos_aop_date = t;
				}
			}

			if (argos_aop_date)
			{
				// Update configuration store
				configuration_store->write_pass_predict(pass_predict);

				// Set to most recent AOP bulletin record
				configuration_store->write_param(ParamID::ARGOS_AOP_DATE, argos_aop_date);

				// Save configuration to commit AOPDATE
				configuration_store->save_params();

				// Log that the PASPW has been updated
				auto time = std::gmtime(&argos_aop_date);
				char buff[256];
				std::strftime(buff, sizeof(buff), "%d/%m/%Y %H:%M:%S", time);
				DEBUG_INFO("DTEHandler:PASPW_REQ: saving PASPW with #AOPs=%u ARGOS_AOP_DATE=%s", (unsigned int)pass_predict.num_records, buff);
				break;
			} else {
				DEBUG_ERROR("DTEHandler::PASPW_REQ: no valid AOP records, so not updating the config store");
				error_code = (int)DTEError::INCORRECT_DATA;
				break;
			}
		}

		return DTEEncoder::encode(DTECommand::PASPW_RESP, error_code);
	}

	std::string DUMPD_REQ(int error_code, std::vector<BaseType>& arg_list, DTEAction& action) {

		action = DTEAction::NONE;

		// A bit of explanation here.  The protocol for DUMPD sends back a payload format of
		// mmm,MMM,<payload>
		// where mmm is a packet index and MMM is the maximum packet index whose value
		// is NNN-1, where NNN is the total number of packets to send.
		if (error_code) {
			// Reset state variables back to zero if an error arises
			m_dumpd_NNN = 0;
			m_dumpd_mmm = 0;
			return DTEEncoder::encode(DTECommand::DUMPD_RESP, error_code);
		}

		Logger *logger = nullptr;

		// Extract the d_type parameter from arg_list to determine which log file to use
		unsigned int d_type = std::get<unsigned int>(arg_list[0]);

		try {
			logger = LoggerManager::find_by_name(m_logger_dump.at(d_type).c_str());
		} catch (...) {
			// Ignore any exceptions -- the logger will be nullptr
		}

		// Either invalid log file or the logger doesn't exist
		if (logger == nullptr) {
			error_code = (int)DTEError::INCORRECT_DATA;
			return DTEEncoder::encode(DTECommand::DUMPD_RESP, error_code);
		}

		// Get the log formatter
		LogFormatter *formatter;
		formatter = logger->get_log_formatter();

		// Check to see if this is the first item
		unsigned int total_entries = logger->num_entries();
		if (0 == m_dumpd_NNN) {
			m_dumpd_NNN = (total_entries + (DTE_HANDLER_MAX_LOG_DUMP_ENTRIES-1)) / DTE_HANDLER_MAX_LOG_DUMP_ENTRIES;
			// Special case where log file is empty we set NNN to 1 and will send an empty payload
			m_dumpd_NNN = m_dumpd_NNN == 0 ? 1 : m_dumpd_NNN;
			m_dumpd_mmm = 0;
		}

		LogEntry log_entry;
		BaseRawData raw_data;
		unsigned int start_index = m_dumpd_mmm * DTE_HANDLER_MAX_LOG_DUMP_ENTRIES;
		unsigned int num_entries = std::min(total_entries - start_index, DTE_HANDLER_MAX_LOG_DUMP_ENTRIES);
		raw_data.ptr = nullptr;
		raw_data.length = 0;

		// Set CSV header line if this is the first packet output
		if (0 == m_dumpd_mmm)
			raw_data.str.append(formatter->header());

		for (unsigned int i = 0; i < num_entries; i++) {
			logger->read(&log_entry, i + start_index);
			// Concatenate formatted log entry
			raw_data.str.append(formatter->log_entry(log_entry));
		}

		// Note that MMM=NNN-1
		std::string msg = DTEEncoder::encode(DTECommand::DUMPD_RESP, error_code, m_dumpd_mmm, m_dumpd_NNN - 1, raw_data);

		m_dumpd_mmm++; // Increment in readiness for next iteration
		if (m_dumpd_mmm == m_dumpd_NNN) {
			m_dumpd_NNN = 0; // Marks the sequence as complete
		} else {
			action = DTEAction::AGAIN;  // Inform caller that we need another iteration
		}

		return msg;
	}

	static std::string ERASE_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::ERASE_RESP, error_code);
		}

		DEBUG_TRACE("Processing ERASE");

		// Extract the d_type parameter from arg_list to determine which log file(s) to erase
		unsigned int d_type = std::get<unsigned int>(arg_list[0]);

		if (d_type == (unsigned int)BaseEraseType::ALL) {
			// Truncate all loggers
			LoggerManager::truncate();
		} else {
			Logger *logger = nullptr;

			try {
				logger = LoggerManager::find_by_name(m_logger_erase.at(d_type).c_str());
			} catch (...) {
				// Ignore any exceptions -- the logger will be nullptr
			}

			if (logger)
			{
				DEBUG_TRACE("Truncating log %s", logger->get_name());
				logger->truncate();
			}
			else
			{
				error_code = (int)DTEError::INCORRECT_DATA;
			}
		}
		return DTEEncoder::encode(DTECommand::ERASE_RESP, error_code);
	}

	static std::string SCALW_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::SCALW_RESP, error_code);
		}

		// Extract the device_id parameter from arg_list to determine which device to calibrate
		unsigned int device_id = std::get<unsigned int>(arg_list[0]);

		// Extract the calibration offset parameter from arg_list to determine which sensor offset to calibrate
		unsigned int offset = std::get<unsigned int>(arg_list[1]);

		// Extract the calibration value parameter from arg_list to use
		double value = std::get<double>(arg_list[2]);

		try {
			const char *name = m_scalx.at(device_id).c_str();
			DEBUG_TRACE("Calibrating device %s...", name);
			Calibratable& s = CalibratableManager::find_by_name(name);
			s.calibration_write(value, offset);
		} catch (...) {
			DEBUG_TRACE("Device calibration failed");
			error_code = (int)DTEError::INCORRECT_DATA;
		}

		return DTEEncoder::encode(DTECommand::SCALW_RESP, error_code);
	}

	static std::string SCALR_REQ(int error_code, std::vector<BaseType>& arg_list) {
		if (error_code) {
			return DTEEncoder::encode(DTECommand::SCALR_RESP, error_code);
		}

		// Extract the device_id parameter from arg_list to determine which device to calibrate
		unsigned int device_id = std::get<unsigned int>(arg_list[0]);

		// Extract the calibration offset parameter from arg_list to determine which sensor offset to calibrate
		unsigned int offset = std::get<unsigned int>(arg_list[1]);

		double value;

		try {
			const char *name = m_scalx.at(device_id).c_str();
			DEBUG_TRACE("Read device %s calibration setting %u...", name, offset);
			Calibratable& s = CalibratableManager::find_by_name(name);
			s.calibration_read(value, offset);
		} catch (...) {
			DEBUG_TRACE("Device calibration read failed");
			error_code = (int)DTEError::INCORRECT_DATA;
		}

		if (error_code) {
			return DTEEncoder::encode(DTECommand::SCALR_RESP, error_code);
		}

		return DTEEncoder::encode(DTECommand::SCALR_RESP, error_code, value);
	}

	std::string ARGOSTX_REQ(int error_code, std::vector<BaseType>& arg_list) {

		if (error_code) {
			return DTEEncoder::encode(DTECommand::ARGOSTX_RESP, error_code);
		}

		// Extract the argos modulation (0=>A2, 1=>A3, 2=>A4)
		ArticMode modulation = (ArticMode)std::get<unsigned int>(arg_list[0]);

		// Extract the argos power level in mW
		BaseArgosPower power = argos_integer_to_power(std::get<unsigned int>(arg_list[1]));

		// Extract the argos frequency (double)
		double freq = std::get<double>(arg_list[2]);

		// Extract the payload size in bytes
		unsigned int num_bytes = std::get<unsigned int>(arg_list[3]);

		// Extract the TCXO warmup time in seconds
		unsigned int tcxo_time = std::get<unsigned int>(arg_list[4]);

		try {
			// If not already active then subscribe to events and setup a sufficiently
			// long idle period before the driver shuts off argos power
			if (!m_artic_device_active) {
				artic_device->subscribe(*this);
				artic_device->set_idle_timeout(30000);
				m_artic_device_active = true;
			}

			// Schedule transmission
			artic_device->set_tx_power(power);
			artic_device->set_tcxo_warmup_time(tcxo_time);
			artic_device->set_frequency(freq);
			ArticPacket packet(0xFF, num_bytes);
			artic_device->send(modulation, packet, 8 * num_bytes);

		} catch (...) {
			error_code = (int)DTEError::INCORRECT_DATA;
		}

		return DTEEncoder::encode(DTECommand::ARGOSTX_RESP, error_code);
	}

	void react(ArticEventPowerOff const& ) {
		if (m_artic_device_active) {
			m_artic_device_active = false;
			artic_device->set_idle_timeout(3000);
			artic_device->unsubscribe(*this);
		}
	}

public:
	void reset_state() {
		m_dumpd_NNN = 0;
		m_dumpd_mmm = 0;
	}

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
			resp = PARMW_REQ(error_code, param_values, action);
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
		case DTECommand::RSTVW_REQ:
			resp = RSTVW_REQ(error_code, arg_list);
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
		case DTECommand::PASPW_REQ:
			resp = PASPW_REQ(error_code, arg_list);
			break;
		case DTECommand::DUMPD_REQ:
			resp = DUMPD_REQ(error_code, arg_list, action);
			break;
		case DTECommand::ERASE_REQ:
			resp = ERASE_REQ(error_code, arg_list);
			break;
		case DTECommand::SCALW_REQ:
			resp = SCALW_REQ(error_code, arg_list);
			break;
		case DTECommand::SCALR_REQ:
			resp = SCALR_REQ(error_code, arg_list);
			break;
		case DTECommand::ARGOSTX_REQ:
			resp = ARGOSTX_REQ(error_code, arg_list);
			break;
		default:
			break;
		}

		return action;
	}

#pragma GCC diagnostic pop

};
