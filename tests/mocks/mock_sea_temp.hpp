#pragma once

#include "sea_temp_sensor.hpp"

#include "CppUTestExt/MockSupport.h"

class MockSeaTempSensor : public SeaTempSensor {

public:
	MockSeaTempSensor(Logger *logger) : SeaTempSensor(logger) {}
	void calibrate(double value, unsigned int offset) override {
		mock().actualCall("calibrate").onObject(this).withParameter("value", value).withParameter("offset", offset);
	}

	double read(unsigned int port) override {
		return mock().actualCall("read").onObject(this).withParameter("port", port).returnDoubleValue();
	}
};
