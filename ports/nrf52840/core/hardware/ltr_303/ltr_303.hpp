#pragma once

#include <stdint.h>
#include "sensor.hpp"

class LTR303 : public Sensor
{
public:
	LTR303();
	double read(unsigned int gain = 1) override;

private:
    void readReg(uint8_t reg, uint8_t *data, size_t len);
    void writeReg(uint8_t address, uint8_t value);

    static constexpr uint32_t STANDBY_TO_ACTIVE_MS = 10;
};
