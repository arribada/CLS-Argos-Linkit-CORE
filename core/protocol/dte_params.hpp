#pragma once

#include "base_types.hpp"

struct ParamValue {
	ParamID  param;
	BaseType value;
};

extern const BaseMap param_map[];
extern const size_t param_map_size;
