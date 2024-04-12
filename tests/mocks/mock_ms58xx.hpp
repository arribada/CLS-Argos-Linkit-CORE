#pragma once

#include "ms58xx.hpp"

#include "CppUTestExt/MockSupport.h"

class MockMS58xx : public MS58xxHardware {
public:
	void read(double& temperature, double& pressure) {
		temperature = mock().actualCall("read.temperature").onObject(this).returnDoubleValue();
		pressure = mock().actualCall("read.pressure").onObject(this).returnDoubleValue();
	}
};
