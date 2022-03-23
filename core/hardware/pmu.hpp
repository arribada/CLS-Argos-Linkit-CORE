#pragma once

#include <string>
#include <cstdint>

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
	static const std::string hardware_version();
	static uint32_t device_identifier();
};
