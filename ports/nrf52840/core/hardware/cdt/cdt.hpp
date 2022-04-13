#pragma once

#include "sensor.hpp"

class CDT : public Sensor {
public:
	CDT() : Sensor("CDT") {}
	void calibrate(double, unsigned int) override {}
	double read(unsigned int) { return 0; }
};
