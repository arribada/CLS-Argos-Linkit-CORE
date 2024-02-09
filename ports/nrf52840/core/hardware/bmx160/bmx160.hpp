#pragma once

#include <functional>
#include <cstdint>
#include "sensor.hpp"
#include "nrf_irq.hpp"

extern "C" {
#include "bmx160_defs.h"
}

class BMX160LL
{
public:
	unsigned int m_bus;
	unsigned char m_addr;
	BMX160LL(unsigned int bus, unsigned char addr, int wakeup_pin);
	~BMX160LL();
	double read_temperature();
	void read_xyz(double& x, double& y, double& z);
	void set_wakeup_threshold(double threshold);
	void set_wakeup_duration(double duration);
	void enable_wakeup(std::function<void()> func);
	void disable_wakeup();
	bool check_and_clear_wakeup();

private:
	NrfIRQ m_irq;
	uint8_t m_unique_id;
	uint8_t m_accel_sleep_mode;
	struct bmx160_dev m_bmx160_dev;
	double m_wakeup_threshold;
	double m_wakeup_slope;
	double m_wakeup_duration;
	bool m_irq_pending;

	void init();
	static int8_t i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
	static int8_t i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t len);
	double convert_g_force(unsigned int g_scale, int16_t axis_value);
};


class BMX160 : public Sensor {
public:
	BMX160();
	void calibration_write(const double, const unsigned int) override;
	double read(unsigned int offset) override;
	void install_event_handler(unsigned int, std::function<void()>) override;
	void remove_event_handler(unsigned int) override;

private:
	BMX160LL m_bmx160;
	double m_last_x;
	double m_last_y;
	double m_last_z;
};
