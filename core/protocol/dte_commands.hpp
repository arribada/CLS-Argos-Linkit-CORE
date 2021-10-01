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
	DUMPD_REQ,
	RSTVW_REQ,
	RSTBW_REQ,
	FACTW_REQ,
	STATR_REQ,
	ERASE_REQ,
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
	DUMPD_RESP,
	RSTVW_RESP,
	RSTBW_RESP,
	FACTW_RESP,
	STATR_RESP,
	ERASE_RESP,
	__NUM_RESP
};

struct DTECommandMap {
	std::string name;
	DTECommand  command;
	std::vector<BaseMap> prototype;
};

static const DTECommandMap command_map[] = {
	{
		.name = "PARML",
		.command = DTECommand::PARML_REQ,
		.prototype = 
		{
		}
	},
	{
		.name = "PARMR",
		.command = DTECommand::PARMR_REQ,
		.prototype = 
		{
			{
				.name = "keys",
				.key = "",
				.encoding = BaseEncoding::KEY_LIST,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PARMW",
		.command = DTECommand::PARMW_REQ,
		.prototype = 
		{
			{
				.name = "key_values",
				.key = "",
				.encoding = BaseEncoding::KEY_VALUE_LIST,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "ZONER",
		.command = DTECommand::ZONER_REQ,
		.prototype = 
		{
			{
				.name = "zone_id",
				.key = "",
				.encoding = BaseEncoding::HEXADECIMAL,
				.min_value = 1U,
				.max_value = 1U,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "ZONEW",
		.command = DTECommand::ZONEW_REQ,
		.prototype = 
		{
			{
				.name = "zone_file",
				.key = "",
				.encoding = BaseEncoding::BASE64,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PROFR",
		.command = DTECommand::PROFR_REQ,
		.prototype = 
		{
		}
	},
	{
		.name = "PROFW",
		.command = DTECommand::PROFW_REQ,
		.prototype = 
		{
			{
				.name = "profile_name",
				.key = "",
				.encoding = BaseEncoding::TEXT,
				.min_value = 1,
				.max_value = 128,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PASPW",
		.command = DTECommand::PASPW_REQ,
		.prototype = 
		{
			{
				.name = "prepass_file",
				.key = "",
				.encoding = BaseEncoding::BASE64,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "SECUR",
		.command = DTECommand::SECUR_REQ,
		.prototype = 
		{
				{
					.name = "accesscode",
					.key = "",
					.encoding = BaseEncoding::HEXADECIMAL,
					.min_value = 0U,
					.max_value = 0U,
					.permitted_values = {},
					.is_implemented = false,
					.is_writable = false
				},
		}
	},
	{
		.name = "DUMPM",
		.command = DTECommand::DUMPM_REQ,
		.prototype = 
		{
			{
				.name = "start_address",
				.key = "",
				.encoding = BaseEncoding::HEXADECIMAL,
				.min_value = 0U,
				.max_value = 0U,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			},
			{
				.name = "length",
				.key = "",
				.encoding = BaseEncoding::HEXADECIMAL,
				.min_value = 0U,
				.max_value = 0x500U,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "DUMPD",
		.command = DTECommand::DUMPD_REQ,
		.prototype = 
		{
			{
				.name = "d_type",
				.key = "",
				.encoding = BaseEncoding::HEXADECIMAL,
				.min_value = 0U,
				.max_value = 1U,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			},
		}
	},
	{
		.name = "RSTVW",
		.command = DTECommand::RSTVW_REQ,
		.prototype =
		{
				{
					.name = "index",
					.key = "",
					.encoding = BaseEncoding::HEXADECIMAL,
					.min_value = 0U,
					.max_value = 0U,
					.permitted_values = { 1U, 3U },
					.is_implemented = false,
					.is_writable = false
				},
		}
	},
	{
		.name = "RSTBW",
		.command = DTECommand::RSTBW_REQ,
		.prototype = 
		{
		}
	},
	{
		.name = "FACTW",
		.command = DTECommand::FACTW_REQ,
		.prototype = 
		{
		}
	},
	{
		.name = "STATR",
		.command = DTECommand::STATR_REQ,
		.prototype =
		{
			{
				.name = "keys",
				.key = "",
				.encoding = BaseEncoding::KEY_LIST,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "ERASE",
		.command = DTECommand::ERASE_REQ,
		.prototype =
		{
			{
				.name = "log_type",
				.key = "",
				.encoding = BaseEncoding::UINT,
				.min_value = 1U,
				.max_value = 3U,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PARML",
		.command = DTECommand::PARML_RESP,
		.prototype = 
		{
			{
				.name = "keys",
				.key = "",
				.encoding = BaseEncoding::KEY_LIST,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PARMR",
		.command = DTECommand::PARMR_RESP,
		.prototype = 
		{
			{
				.name = "key_values",
				.key = "",
				.encoding = BaseEncoding::KEY_VALUE_LIST,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PARMW",
		.command = DTECommand::PARMW_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "ZONER",
		.command = DTECommand::ZONER_RESP,
		.prototype = 
		{
			{
				.name = "zone_file",
				.key = "",
				.encoding = BaseEncoding::BASE64,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "ZONEW",
		.command = DTECommand::ZONEW_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "PROFR",
		.command = DTECommand::PROFR_RESP,
		.prototype = 
		{
			{
				.name = "profile_name",
				.key = "",
				.encoding = BaseEncoding::TEXT,
				.min_value = "",
				.max_value = "",
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "PROFW",
		.command = DTECommand::PROFW_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "PASPW",
		.command = DTECommand::PASPW_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "SECUR",
		.command = DTECommand::SECUR_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "DUMPM",
		.command = DTECommand::DUMPM_RESP,
		.prototype = 
		{
			{
				.name = "data",
				.key = "",
				.encoding = BaseEncoding::BASE64,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "DUMPD",
		.command = DTECommand::DUMPD_RESP,
		.prototype = 
		{
			{
				.name = "mmm",
				.key = "",
				.encoding = BaseEncoding::HEXADECIMAL,
				.min_value = 0U,
				.max_value = 0xFFFU,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			},
			{
				.name = "MMM",
				.key = "",
				.encoding = BaseEncoding::HEXADECIMAL,
				.min_value = 0U,
				.max_value = 0xFFFU,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			},
			{
				.name = "data",
				.key = "",
				.encoding = BaseEncoding::BASE64,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "RSTVW",
		.command = DTECommand::RSTVW_RESP,
		.prototype =
		{
		}
	},
	{
		.name = "RSTBW",
		.command = DTECommand::RSTBW_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "FACTW",
		.command = DTECommand::FACTW_RESP,
		.prototype = 
		{
		}
	},
	{
		.name = "STATR",
		.command = DTECommand::STATR_RESP,
		.prototype =
		{
			{
				.name = "key_values",
				.key = "",
				.encoding = BaseEncoding::KEY_VALUE_LIST,
				.min_value = 0,
				.max_value = 0,
				.permitted_values = {},
				.is_implemented = false,
				.is_writable = false
			}
		}
	},
	{
		.name = "ERASE",
		.command = DTECommand::ERASE_RESP,
		.prototype =
		{
		}
	},
};

#endif // __DTE_COMMANDS_HPP_
