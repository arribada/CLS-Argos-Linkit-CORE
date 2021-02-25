#include "pmu.hpp"

#include "CppUTestExt/MockSupport.h"


void PMU::reset(bool dfu_mode) {
	mock().actualCall("reset").withParameter("dfu_mode", dfu_mode);
}

void PMU::powerdown() {
	mock().actualCall("powerdown");
}
