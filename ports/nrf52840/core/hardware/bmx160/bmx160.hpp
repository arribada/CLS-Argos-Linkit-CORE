#pragma once

#include <cstdint>
#include "sensor.hpp"

extern "C" {
#include "bmx160_defs.h"
}

class BMX160LL
{
public:
	unsigned int m_bus;
	unsigned char m_addr;
	BMX160LL(unsigned int bus, unsigned char addr);
	~BMX160LL();
	double read_temperature();
	void read_xyz(double& x, double& y, double& z);

private:
	uint8_t m_unique_id;
	struct bmx160_dev m_bmx160_dev;

	void init();
	static int8_t i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
	static int8_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
	double convert_g_force(unsigned int g_scale, int16_t axis_value);

};


class BMX160 : public Sensor {
public:
	BMX160();
	void calibrate(double, unsigned int) override;
	double read(unsigned int offset) override;

private:
	BMX160LL m_bmx160;
	double m_last_x;
	double m_last_y;
	double m_last_z;
};
