#pragma once

#include "battery.hpp"
extern "C" {
	#include "stc311x_BatteryInfo.h"
	#include "stc311x_gasgauge.h"
}

class GaugeBatteryMonitor : public BatteryMonitor {
private:
	bool m_is_init;
	GasGauge_DataTypeDef STC3117_GG_struct;

    int check_i2c_device();
	void internal_update() override;

public:
	GaugeBatteryMonitor(
			uint16_t critical_voltage = 2800,
			uint8_t low_level = 10);
    bool IsInit() { return m_is_init; };
	
	// Static C++ I2C functions to forward to the C driver
    static int i2c_write(int I2cSlaveAddr, int RegAddress, unsigned char* TxBuffer, int NumberOfBytes);
    static int i2c_read(int I2cSlaveAddr, int RegAddress, unsigned char* RxBuffer, int NumberOfBytes);

};
