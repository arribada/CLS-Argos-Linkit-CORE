#include "stwlc68.hpp"
#include "bsp.hpp"
#include "debug.hpp"

extern "C" {
#include "otp.h"
#include "otphal.h"
}

using namespace std::string_literals;

void STWLC68::init() {
	otphal_init(&BSP::I2C_Inits[BSP::I2C::I2C_1], BSP::GPIO_Inits[BSP::GPIO::GPIO_WCHG_INTB].pin_number, STWLC68_ADDRESS, nullptr);
}

std::string STWLC68::get_chip_status() {
	int ret = is_otp_programmed();
	DEBUG_TRACE("STWLC68::get_chip_status: is_otp_programmed()=%d", ret);
	if (ret == OK) {
		return "PROGRAMMED"s;
	} else if (ret == -1) {
		return "UNDETECTED"s;
	} else if (ret == -2) {
		return "NEEDSOTP"s;
	} else {
		return "UNKNOWN"s;
	}
}
