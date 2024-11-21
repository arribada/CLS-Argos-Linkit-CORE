#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include "sensor.hpp"
#include "bsp.hpp"
#include "error.hpp"


class PressureSensorDevice {
public:
    virtual ~PressureSensorDevice() {}
    virtual void read(double& temperature, double& pressure) = 0;
};

class PressureSensoDummyDevice : public PressureSensorDevice {
public:
    void read(double& t, double& p) { t = 0; p = 0; }
};

class PressureSensor : public Sensor {
public:
    PressureSensor(PressureSensorDevice& device) : Sensor("PRS"), m_device(device) {}
    double read(unsigned int channel = 0) {
        if (0 == channel) {
            m_device.read(m_last_temperature, m_last_pressure);
            return m_last_pressure;
        } else if (1 == channel) {
            return m_last_temperature;
        }
        throw ErrorCode::BAD_SENSOR_CHANNEL;
    }

private:
    double m_last_pressure;
    double m_last_temperature;
    PressureSensorDevice& m_device;
};
