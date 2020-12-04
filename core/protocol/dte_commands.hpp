#ifndef __DTE_COMMANDS_HPP_
#define __DTE_COMMANDS_HPP_

#include "base_types.hpp"


#define RESP_CMD_BASE   0x80

enum class DTECommand {
	PARML_REQ,
	PARMR_REQ,
	PARMW_REQ,
	ZONER_REQ,
	ZONEW_REQ,
	PROFR_REQ,
	PROFW_REQ,
	PASPW_REQ,
	SECUR_REQ,
	DUMPM_REQ,
	DUMPL_REQ,
	DUMPD_REQ,
	RESET_REQ,
	FACTR_REQ,
	__NUM_REQ,
	PARML_RESP = RESP_CMD_BASE,
	PARMR_RESP,
	PARMW_RESP,
	ZONER_RESP,
	ZONEW_RESP,
	PROFR_RESP,
	PROFW_RESP,
	PASPW_RESP,
	SECUR_RESP,
	DUMPM_RESP,
	DUMPL_RESP,
	DUMPD_RESP,
	RESET_RESP,
	FACTR_RESP,
	__NUM_RESP
};

struct DTECommandMap {
	std::string name;
	DTECommand  command;
	std::vector<BaseMap> prototype;
};

static const DTECommandMap command_map[] = {
	{
		"PARML",
		DTECommand::PARML_REQ,
		{
		}
	},
	{
		"PARMR",
		DTECommand::PARMR_REQ,
		{
			{ "keys", "", BaseEncoding::KEY_LIST, 0, 0, {} }
		}
	},
	{
		"PARMW",
		DTECommand::PARMW_REQ,
		{
			{ "key_values", "", BaseEncoding::KEY_VALUE_LIST, 0, 0, {} }
		}
	},
	{
		"ZONER",
		DTECommand::ZONER_REQ,
		{
			{ "zone_id", "", BaseEncoding::HEXADECIMAL, 1U, 1U, {} }
		}
	},
	{
		"ZONEW",
		DTECommand::ZONEW_REQ,
		{
			{ "zone_file", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"PROFR",
		DTECommand::PROFR_REQ,
		{
		}
	},
	{
		"PROFW",
		DTECommand::PROFW_REQ,
		{
			{ "profile_name", "", BaseEncoding::TEXT, 1, 128, {} }
		}
	},
	{
		"PASPW",
		DTECommand::PASPW_REQ,
		{
			{ "prepass_file", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"SECUR",
		DTECommand::SECUR_REQ,
		{
		}
	},
	{
		"DUMPM",
		DTECommand::DUMPM_REQ,
		{
			{ "start_address", "", BaseEncoding::HEXADECIMAL, 0U, 0U, {} },
			{ "length", "", BaseEncoding::HEXADECIMAL, 0U, 0x500U, {} }
		}
	},
	{
		"DUMPL",
		DTECommand::DUMPL_REQ,
		{
		}
	},
	{
		"DUMPD",
		DTECommand::DUMPD_REQ,
		{
		}
	},
	{
		"RESET",
		DTECommand::RESET_REQ,
		{
		}
	},
	{
		"FACTR",
		DTECommand::FACTR_REQ,
		{
		}
	},
	{
		"PARML",
		DTECommand::PARML_RESP,
		{
			{ "keys", "", BaseEncoding::KEY_LIST, 0, 0, {} }
		}
	},
	{
		"PARMR",
		DTECommand::PARMR_RESP,
		{
			{ "key_values", "", BaseEncoding::KEY_VALUE_LIST, 0, 0, {} }
		}
	},
	{
		"PARMW",
		DTECommand::PARMW_RESP,
		{
		}
	},
	{
		"ZONER",
		DTECommand::ZONER_RESP,
		{
			{ "zone_file", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"ZONEW",
		DTECommand::ZONEW_RESP,
		{
		}
	},
	{
		"PROFR",
		DTECommand::PROFR_RESP,
		{
			{ "profile_name", "", BaseEncoding::TEXT, "", "", {} }
		}
	},
	{
		"PROFW",
		DTECommand::PROFW_RESP,
		{
		}
	},
	{
		"PASPW",
		DTECommand::PASPW_RESP,
		{
		}
	},
	{
		"SECUR",
		DTECommand::SECUR_RESP,
		{
		}
	},
	{
		"DUMPM",
		DTECommand::DUMPM_RESP,
		{
			{ "data", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPL",
		DTECommand::DUMPL_RESP,
		{
			{ "data", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPD",
		DTECommand::DUMPD_RESP,
		{
			{ "data", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"RESET",
		DTECommand::RESET_RESP,
		{
		}
	},
	{
		"FACTR",
		DTECommand::FACTR_RESP,
		{
		}
	}
};

#endif // __DTE_COMMANDS_HPP_
