#include "sensor.hpp"

Sensor::Sensor(const char *name) : Calibratable(name) {
	SensorManager::add(*this, name);
}

Sensor::~Sensor() {
	SensorManager::remove(*this);
}
