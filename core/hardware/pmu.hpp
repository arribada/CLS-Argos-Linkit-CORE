#pragma once

#include <string>

class PMU {
public:
	static void initialise();
	static void reset(bool dfu_mode);
	static void powerdown();
	static void run();
	static void delay_ms(unsigned ms);
	static void delay_us(unsigned us);
	static void start_watchdog();
	static void kick_watchdog();
	static const std::string reset_cause();
};
