#include <stdint.h>
#include "otphal.h"
#include "nrf_delay.h"
#include "nrfx_twim.h"
#include "nrf_gpio.h"

// Local contexts must be set via otphal_init
static nrfx_twim_t const * m_twim_instance;
static uint32_t m_int_status_pin_number;
static uint8_t m_i2c_slave_addr;

//#define I2C_SLAVE_ADDR 		0x61


void otphal_init(void const * p_instance, uint32_t int_status_pin_number, uint8_t i2c_slave_addr) {
	m_twim_instance = (nrfx_twim_t const *)p_instance;
	m_int_status_pin_number = int_status_pin_number;
	m_i2c_slave_addr = i2c_slave_addr;
}

void otphal_sleep_ms(uint32_t ms) {
    nrf_delay_ms(ms);
}

int otphal_i2c_write(uint8_t* data, int data_length) {
	nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TX(m_i2c_slave_addr, data, data_length);
	if (nrfx_twim_xfer(m_twim_instance, &desc, 0) != NRFX_SUCCESS)
		return -1;
	return 0;
}

int otphal_i2c_write_read(uint8_t* cmd, int cmd_length, uint8_t* read_data, int count) {
	nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TXRX(m_i2c_slave_addr, cmd, cmd_length, read_data, count);
	if (nrfx_twim_xfer(m_twim_instance, &desc, 0) != NRFX_SUCCESS)
		return -1;
	return 0;
}

int otphal_gpio_get_value(void) {
	return nrf_gpio_pin_read(m_int_status_pin_number);
}
