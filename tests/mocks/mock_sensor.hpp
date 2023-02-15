#pragma once

#include "sensor.hpp"

#include "CppUTestExt/MockSupport.h"

class MockSensor : public Sensor {

public:
	MockSensor(const char *name = "Mock") : Sensor(name) {}

	void calibration_write(const double value, const unsigned int offset) override {
		mock().actualCall("calibration_write").onObject(this).withParameter("value", value).withParameter("offset", offset);
	}

	double read(unsigned int port) override {
		return mock().actualCall("read").onObject(this).withParameter("port", port).returnDoubleValue();
	}
};
