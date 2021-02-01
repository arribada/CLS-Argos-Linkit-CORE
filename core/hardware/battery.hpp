#ifndef __BATTERY_HPP_
#define __BATTERY_HPP_

#include <stdint.h>

class BatteryMonitor {
public:
	virtual ~BatteryMonitor() {}
	virtual void start() = 0;
	virtual void stop() = 0;
	virtual uint16_t get_voltage() = 0;
	virtual uint8_t get_level() = 0;
};

#endif // __BATTERY_HPP_
