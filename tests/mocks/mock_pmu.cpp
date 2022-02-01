#include "pmu.hpp"

#include "CppUTestExt/MockSupport.h"


void PMU::reset(bool dfu_mode) {
	mock().actualCall("reset").withParameter("dfu_mode", dfu_mode);
}

void PMU::powerdown() {
	mock().actualCall("powerdown");
}

void PMU::delay_ms(unsigned ms) {
	mock().actualCall("delay_ms").withParameter("ms", ms);
}

void PMU::delay_us(unsigned us) {
	mock().actualCall("delay_us").withParameter("us", us);
}

void PMU::start_watchdog() {
	mock().actualCall("start_watchdog");
}

void PMU::kick_watchdog() {
	mock().actualCall("kick_watchdog");
}

const std::string PMU::reset_cause() {
	return "UNKNOWN";
}

const std::string PMU::hardware_version() {
	return "SIMULATOR";
}
