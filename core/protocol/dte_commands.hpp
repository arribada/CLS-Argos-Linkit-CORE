#ifndef __DTE_COMMANDS_HPP_
#define __DTE_COMMANDS_HPP_

#include <string>
#include <vector>
#include <variant>

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
	FACTR_RESP
};

enum class DTECommandArgType {
	DECIMAL,
	HEXADECIMAL,
	BASE64,
	TEXT,
	KEY_LIST,
	KEY_VALUE_LIST
};

using DTECommandArgConstraint = std::variant<int, float>;

struct DTECommandArgMap {
	std::string name;
	DTECommandArgType type;
	DTECommandArgConstraint min_value;
	DTECommandArgConstraint max_value;
	std::vector<DTECommandArgConstraint> permitted_values;
};

struct DTECommandMap {
	std::string name;
	std::vector<DTECommandArgMap> prototype;
};

static const DTECommandMap command_map[] = {
	{
		"PARML",
		{
		}
	},
	{
		"PARMR",
		{
			{ "keys", DTECommandArgType::KEY_LIST, 0, 0, {} }
		}
	},
	{
		"PARMW",
		{
			{ "key_values", DTECommandArgType::KEY_VALUE_LIST, 0, 0, {} }
		}
	},
	{
		"ZONER",
		{
			{ "zone_id", DTECommandArgType::KEY_LIST, 1, 1, {} }
		}
	},
	{
		"ZONEW",
		{
			{ "zone_file", DTECommandArgType::BASE64, 0, 0, {} }
		}
	},
	{
		"PROFR",
		{
		}
	},
	{
		"PROFW",
		{
			{ "profile_name", DTECommandArgType::TEXT, 1, 128, {} }
		}
	},
	{
		"PASPW",
		{
			{ "prepass_file", DTECommandArgType::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPM",
		{
			{ "start_address", DTECommandArgType::HEXADECIMAL, 0, 0, {} },
			{ "length", DTECommandArgType::HEXADECIMAL, 0, 0x500, {} }
		}
	},
	{
		"DUMPL",
		{
		}
	},
	{
		"DUMPD",
		{
		}
	},
	{
		"RESET",
		{
		}
	},
	{
		"FACTR",
		{
		}
	},
	{
		"PARML",
		{
			{ "keys", DTECommandArgType::KEY_LIST, 0, 0, {} }
		}
	},
	{
		"PARMR",
		{
			{ "key_values", DTECommandArgType::KEY_VALUE_LIST, 0, 0, {} }
		}
	},
	{
		"PARMW",
		{
		}
	},
	{
		"ZONER",
		{
			{ "zone_file", DTECommandArgType::BASE64, 0, 0, {} }
		}
	},
	{
		"ZONEW",
		{
		}
	},
	{
		"PROFR",
		{
			{ "profile_name", DTECommandArgType::TEXT, 1, 128, {} }
		}
	},
	{
		"PROFW",
		{
		}
	},
	{
		"PASPW",
		{
		}
	},
	{
		"DUMPM",
		{
			{ "data", DTECommandArgType::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPL",
		{
			{ "data", DTECommandArgType::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPD",
		{
			{ "data", DTECommandArgType::BASE64, 0, 0, {} }
		}
	},
	{
		"RESET",
		{
		}
	},
	{
		"FACTR",
		{
		}
	}
};

#endif // __DTE_COMMANDS_HPP_
