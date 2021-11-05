#ifndef __DTE_HANDLER_HPP_
#define __DTE_HANDLER_HPP_

#include <algorithm>

#include "dte_protocol.hpp"
#include "config_store.hpp"
#include "memory_access.hpp"
#include "timeutils.hpp"

using namespace std::literals::string_literals;


class LogFormatter {
public:
	virtual ~LogFormatter() {}
	virtual const std::string header() = 0;
	virtual const std::string log_entry(const LogEntry& e) = 0;
};


class SysLogFormatter : public LogFormatter {
private:
	const char *log_level_str(LogType t) {
		switch (t) {
		case LogType::LOG_ERROR:
			return "ERROR";
		case LogType::LOG_WARN:
			return "WARN";
		case LogType::LOG_INFO:
			return "INFO";
		case LogType::LOG_TRACE:
			return "TRACE";
		default:
		case LogType::LOG_GPS:
		case LogType::LOG_STARTUP:
		case LogType::LOG_ARTIC:
		case LogType::LOG_UNDERWATER:
		case LogType::LOG_BATTERY:
		case LogType::LOG_STATE:
		case LogType::LOG_ZONE:
		case LogType::LOG_OTA_UPDATE:
		case LogType::LOG_BLE:
			return "UNKNOWN";
		}
	}
public:
	const std::string header() override {
		return "log_datetime,log_level,message\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(e.header.year, e.header.month, e.header.day, e.header.hours, e.header.minutes, e.header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		snprintf(entry, sizeof(entry), "%s,%s,%s\r\n",
				d1,
				log_level_str(e.header.log_type),
				e.data);
		return std::string(entry);
	}
};

class GPSLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,batt_voltage,iTOW,fix_datetime,valid,onTime,ttff,fixType,flags,flags2,flags3,numSV,lon,lat,height,hMSL,hAcc,vAcc,velN,velE,velD,gSpeed,headMot,sAcc,headAcc,pDOP,vDOP,hDOP,headVeh\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128], d2[128];
		const GPSLogEntry *gps = (const GPSLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(gps->header.year, gps->header.month, gps->header.day, gps->header.hours, gps->header.minutes, gps->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);
		t = convert_epochtime(gps->info.year, gps->info.month, gps->info.day, gps->info.hour, gps->info.min, gps->info.sec);
		tm = std::gmtime(&t);
		std::strftime(d2, sizeof(d2), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f,%u,%s,%u,%u,%u,%u,%u,%u,%u,%u,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n",
				d1,
				(double)gps->info.batt_voltage/1000,
				(unsigned int)gps->info.iTOW,
				d2,
				(unsigned int)gps->info.valid,
				(unsigned int)gps->info.onTime,
				(unsigned int)gps->info.ttff,
				(unsigned int)gps->info.fixType,
				(unsigned int)gps->info.flags,
				(unsigned int)gps->info.flags2,
				(unsigned int)gps->info.flags3,
				(unsigned int)gps->info.numSV,
				gps->info.lon,
				gps->info.lat,
				(double)gps->info.height / 1000,
				(double)gps->info.hMSL / 1000,
				(double)gps->info.hAcc / 1000,
				(double)gps->info.vAcc / 1000,
				(double)gps->info.velN / 1000,
				(double)gps->info.velE / 1000,
				(double)gps->info.velD / 1000,
				(double)gps->info.gSpeed / 1000,
				(double)gps->info.headMot,
				(double)gps->info.sAcc / 1000,
				(double)gps->info.headAcc,
				(double)gps->info.pDOP,
				(double)gps->info.vDOP,
				(double)gps->info.hDOP,
				(double)gps->info.headVeh);
		return std::string(entry);
	}
};

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
extern Logger *sensor_log;
extern Logger *system_log;

class DTEHandler {
private:
	unsigned int m_dumpd_NNN;
	unsigned int m_dumpd_mmm;
	GPSLogFormatter m_gps_formatter;
	SysLogFormatter m_sys_formatter;

public:
	DTEHandler() {
		m_dumpd_NNN = 0;
		m_dumpd_mmm = 0;
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

	static std::string ZONEW_REQ(int error_code, std::vector<BaseType>& arg_list) {
		while (!error_code) {
			BaseZone zone;
			std::string zone_bits = std::get<std::string>(arg_list[0]);
			try {
				ZoneCodec::decode(zone_bits, zone);
			} catch (ErrorCode e) {
				error_code = (int)DTEError::INCORRECT_DATA;
				break;  // Do not write configuration store
			}
			configuration_store->write_zone(zone);
			break;
		}

		return DTEEncoder::encode(DTECommand::ZONEW_RESP, error_code);
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

			// Update configuration store
			configuration_store->write_pass_predict(pass_predict);

			// Use the first AOP entry to set the AOP last updated date
			std::time_t argos_aop_date = convert_epochtime(pass_predict.records[0].bulletin.year, pass_predict.records[0].bulletin.month, pass_predict.records[0].bulletin.day, pass_predict.records[0].bulletin.hour, pass_predict.records[0].bulletin.minute, pass_predict.records[0].bulletin.second);
			configuration_store->write_param(ParamID::ARGOS_AOP_DATE, argos_aop_date);

			// Save configuration to commit AOPDATE
			configuration_store->save_params();
			break;
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

		Logger *logger;
		LogFormatter *formatter;

		// Extract the d_type parameter from arg_list to determine which log file to use
		unsigned int d_type = std::get<unsigned int>(arg_list[0]);
		if ((unsigned int)BaseLogDType::INTERNAL == d_type)
		{
			logger = system_log;
			formatter = &m_sys_formatter;
		} else {
			logger = sensor_log;
			formatter = &m_gps_formatter;
		}

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

		Logger *logger;

		DEBUG_TRACE("Processing ERASE");

		// Extract the d_type parameter from arg_list to determine which log file(s) to erase
		unsigned int d_type = std::get<unsigned int>(arg_list[0]);
		if ((unsigned int)BaseEraseType::SYSTEM == d_type || (unsigned int)BaseEraseType::SENSOR_AND_SYSTEM == d_type)
		{
			DEBUG_TRACE("Truncating system log");
			logger = system_log;
			logger->truncate();
		}
		if ((unsigned int)BaseEraseType::SENSOR == d_type || (unsigned int)BaseEraseType::SENSOR_AND_SYSTEM == d_type)
		{
			DEBUG_TRACE("Truncating sensor log");
			logger = sensor_log;
			logger->truncate();
		}

		return DTEEncoder::encode(DTECommand::ERASE_RESP, error_code);
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
		case DTECommand::ERASE_REQ:
			resp = ERASE_REQ(error_code, arg_list);
			break;
		default:
			break;
		}

		return action;
	}

#pragma GCC diagnostic pop

};

#endif // __DTE_HANDLER_HPP_
