#include "pmu.hpp"

#include "CppUTestExt/MockSupport.h"


void PMU::reset() {
	mock().actualCall("reset");
}

void PMU::powerdown() {
	mock().actualCall("powerdown");
}
