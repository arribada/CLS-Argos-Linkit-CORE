#include "stwlc68.hpp"
#include "bsp.hpp"

extern "C" {
#include "otp.h"
#include "otphal.h"
}

using STWLC68ChipInfo = struct chip_info;

void STWLC68::init() {
	otphal_init(&BSP::I2C_Inits[BSP::I2C::I2C_1], BSP::GPIO_Inits[BSP::GPIO::GPIO_WCHG_INTB].pin_number, STWLC68_ADDRESS);
}

std::string STWLC68::get_chip_status() {
	STWLC68ChipInfo chip_info;
	int ret = get_chip_info(&chip_info);
	if (ret == OK) {
		return "CHIP_ID=" + std::to_string(chip_info.chip_id) + "CHIP_REV=" + std::to_string(chip_info.chip_revision);
	} else {
		return "UNDETECTED";
	}
}
