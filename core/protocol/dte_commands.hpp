#pragma once

#include "base_types.hpp"


#define RESP_CMD_BASE   0x80

enum class DTECommand {
	PARML_REQ,
	PARMR_REQ,
	PARMW_REQ,
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
	SCALW_REQ,
	ARGOSTX_REQ,
	__NUM_REQ,
	PARML_RESP = RESP_CMD_BASE,
	PARMR_RESP,
	PARMW_RESP,
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
	SCALW_RESP,
	ARGOSTX_RESP,
	__NUM_RESP
};

struct DTECommandMap {
	std::string name;
	DTECommand  command;
	std::vector<BaseMap> prototype;
};

extern const DTECommandMap command_map[];
extern const size_t command_map_size;
