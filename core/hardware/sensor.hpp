#pragma once

class Sensor {
public:
	virtual ~Sensor() {}
	virtual void calibrate(double value, unsigned int offset) = 0;
	virtual double read(unsigned int port = 0) = 0;
};
