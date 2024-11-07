#pragma once

#include <cmath>
#include "ad5933.hpp"

#include "CppUTestExt/MockSupport.h"

class MockAD5933 : public AD5933 {
public:
	void start(const unsigned int frequency = 90000, const VRange vrange = VRange::V1_GAIN1X) {
		mock().actualCall("start").onObject(this).
				withParameter("frequency", frequency).
				withParameter("vrange", (unsigned int)vrange);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	double get_impedence(const unsigned int averaging = 1, const double gain = 1) {
		int16_t real, imag;
		double impedence = 0;
		for (unsigned int i = 0; i < averaging; i++) {
			get_real_imaginary(real, imag);
			double magnitude = std::sqrt(std::pow((double)real, 2) + std::pow((double)imag, 2));
			impedence += 1 / (gain * magnitude);
		}
		impedence /= averaging;
		return impedence;
	}
	void get_real_imaginary(int16_t& real, int16_t& imag) {
		real = (int16_t)mock().actualCall("get_real_imaginary.real").onObject(this).
				returnIntValue();
		imag = (int16_t)mock().actualCall("get_real_imaginary.imag").onObject(this).
				returnIntValue();
	}
};
