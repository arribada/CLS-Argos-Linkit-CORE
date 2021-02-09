#pragma once

#include <string>

enum class RGBLedColor {
	BLACK,
	RED,
	GREEN,
	BLUE,
	CYAN,
	MAGENTA,
	YELLOW,
	WHITE,
};

class RGBLed {

public:
	static std::string color_to_string(RGBLedColor color) {
		switch(color) {
		case RGBLedColor::BLACK:
			return "BLACK";
		case RGBLedColor::RED:
			return "RED";
		case RGBLedColor::GREEN:
			return "GREEN";
		case RGBLedColor::BLUE:
			return "BLUE";
		case RGBLedColor::CYAN:
			return "CYAN";
		case RGBLedColor::MAGENTA:
			return "MAGENTA";
		case RGBLedColor::YELLOW:
			return "YELLOW";
		case RGBLedColor::WHITE:
			return "WHITE";
		default:
			return "UNKNOWN";
		}
	}

	RGBLed() { }
	virtual ~RGBLed() {}
	virtual void off() = 0;
	virtual RGBLedColor get_state() = 0;
	virtual void set(RGBLedColor color) = 0;
	virtual void flash(RGBLedColor color) = 0;
	virtual bool is_flashing() = 0;
};
