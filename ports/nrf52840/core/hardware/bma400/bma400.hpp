#pragma once

#include <functional>
#include <cstdint>
#include "sensor.hpp"
#include "nrf_irq.hpp"

extern "C" {
#include "bma400_defs.h"
}

/*! Read write length varies based on user requirement */
#define BMA400_READ_WRITE_LENGTH  UINT8_C(64)

class BMA400LL
{
public:
	unsigned int m_bus;
	unsigned char m_addr;
	BMA400LL(unsigned int bus, unsigned char addr, int wakeup_pin);
	~BMA400LL();
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
	struct bma400_dev m_bma400_dev;
	double m_wakeup_threshold;
	double m_wakeup_slope;
	double m_wakeup_duration;
	bool m_irq_pending;
	struct bma400_sensor_conf m_bma400_sensor_conf;
	struct bma400_device_conf m_bma400_device_conf;
    struct bma400_int_enable m_bma400_int_en; // interrupt to be declared
	
	void init();
	static int8_t i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr);
	static int8_t i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr);
	static void delay_us(uint32_t period, void *intf_ptr);
	double convert_g_force(unsigned int g_scale, int16_t axis_value);
    void bma400_check_rslt(const char api_name[], int8_t rslt);
};


class BMA400 : public Sensor {
public:
	BMA400();
	void calibration_write(const double, const unsigned int) override;
	double read(unsigned int offset) override;
	void install_event_handler(unsigned int, std::function<void()>) override;
	void remove_event_handler(unsigned int) override;

private:
	BMA400LL m_bma400;
	double m_last_x;
	double m_last_y;
	double m_last_z;
};
