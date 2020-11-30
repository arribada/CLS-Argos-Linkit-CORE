#ifndef __DTE_COMMANDS_HPP_
#define __DTE_COMMANDS_HPP_

#include <string>
#include <vector>
#include <variant>

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
	std::vector<BaseMap> prototype;
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
			{ "keys", "", BaseEncoding::KEY_LIST, 0, 0, {} }
		}
	},
	{
		"PARMW",
		{
			{ "key_values", "", BaseEncoding::KEY_VALUE_LIST, 0, 0, {} }
		}
	},
	{
		"ZONER",
		{
			{ "zone_id", "", BaseEncoding::KEY_LIST, 1, 1, {} }
		}
	},
	{
		"ZONEW",
		{
			{ "zone_file", "", BaseEncoding::BASE64, 0, 0, {} }
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
			{ "profile_name", "", BaseEncoding::TEXT, 1, 128, {} }
		}
	},
	{
		"PASPW",
		{
			{ "prepass_file", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"SECUR",
		{
		}
	},
	{
		"DUMPM",
		{
			{ "start_address", "", BaseEncoding::HEXADECIMAL, 0U, 0U, {} },
			{ "length", "", BaseEncoding::HEXADECIMAL, 0U, 0x500U, {} }
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
			{ "keys", "", BaseEncoding::KEY_LIST, 0, 0, {} }
		}
	},
	{
		"PARMR",
		{
			{ "key_values", "", BaseEncoding::KEY_VALUE_LIST, 0, 0, {} }
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
			{ "zone_file", "", BaseEncoding::BASE64, 0, 0, {} }
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
			{ "profile_name", "", BaseEncoding::TEXT, "", "", {} }
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
		"SECUR",
		{
		}
	},
	{
		"DUMPM",
		{
			{ "data", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPL",
		{
			{ "data", "", BaseEncoding::BASE64, 0, 0, {} }
		}
	},
	{
		"DUMPD",
		{
			{ "data", "", BaseEncoding::BASE64, 0, 0, {} }
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
