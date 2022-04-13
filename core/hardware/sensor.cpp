#include "sensor.hpp"

Sensor::Sensor(const char *name) {
	SensorManager::add(*this, name);
}

Sensor::~Sensor() {
	SensorManager::remove(*this);
}
