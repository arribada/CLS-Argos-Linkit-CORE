#pragma once

#include "sensor.hpp"

#include "CppUTestExt/MockSupport.h"

class MockSensor : public Sensor {

public:
	MockSensor(const char *name = "Mock") : Sensor(name) {}
	void calibrate(double value, unsigned int offset) override {
		mock().actualCall("calibrate").onObject(this).withParameter("value", value).withParameter("offset", offset);
	}

	double read(unsigned int port) override {
		return mock().actualCall("read").onObject(this).withParameter("port", port).returnDoubleValue();
	}
};