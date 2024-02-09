#include <stdint.h>
#include "otphal.h"
#include "nrf_delay.h"
#include "nrfx_twim.h"
#include "nrf_gpio.h"

// Local contexts must be set via otphal_init
static nrfx_twim_t const * m_twim_instance;
static uint32_t m_int_status_pin_number;
static uint8_t m_i2c_slave_addr;
static void (*m_twim_reset)(void) = NULL;

void otphal_init(void const * p_instance,
		uint32_t int_status_pin_number,
		uint8_t i2c_slave_addr,
		void (*reset)(void)) {
	m_twim_instance = (nrfx_twim_t const *)p_instance;
	m_int_status_pin_number = int_status_pin_number;
	m_i2c_slave_addr = i2c_slave_addr;
	m_twim_reset = reset;
}

void otphal_sleep_ms(uint32_t ms) {
    nrf_delay_ms(ms);
}

void otphal_i2c_reset(void) {
	if (m_twim_reset) {
		m_twim_reset();
	}
}

int otphal_i2c_write(uint8_t* data, int data_length) {
	//printf("otphal_i2c_write:nrfx_twim_tx\n");
	if (nrfx_twim_tx(m_twim_instance, m_i2c_slave_addr, data, data_length, false) != NRFX_SUCCESS)
		return -1;
	return 0;
}

int otphal_i2c_write_read(uint8_t* cmd, int cmd_length, uint8_t* read_data, int count) {
	//printf("otphal_i2c_write_read\n");
	if (nrfx_twim_tx(m_twim_instance, m_i2c_slave_addr, cmd, cmd_length, true) != NRFX_SUCCESS)
		return -1;
	//printf("otphal_i2c_write_read:nrfx_twim_rx\n");
	if (nrfx_twim_rx(m_twim_instance, m_i2c_slave_addr, read_data, count) != NRFX_SUCCESS)
		return -1;
	return 0;
}

int otphal_gpio_get_value(void) {
	return nrf_gpio_pin_read(m_int_status_pin_number);
}
