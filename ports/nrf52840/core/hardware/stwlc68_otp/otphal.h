#ifndef OTPHAL_H
#define OTPHAL_H

#include <stdint.h>

void otphal_init(void const * p_instance, uint32_t int_status_pin_number, uint8_t i2c_slave_addr);
int otphal_i2c_write(uint8_t* data, int data_length);
int otphal_i2c_write_read(uint8_t* cmd, int cmd_length, uint8_t* read_data, int count);
int otphal_gpio_get_value(void);
void otphal_sleep_ms(uint32_t ms);

#endif /* OTPHAL_H*/
