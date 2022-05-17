#pragma once

#include "../ms58xx/ms58xx.hpp"
#include "sensor.hpp"
#include "calibration.hpp"
#include "ad5933.hpp"

class CDT : public Sensor {
public:
	CDT(unsigned int bus = EXT_I2C_BUS);
	void calibrate(double, unsigned int) override;
	double read(unsigned int offset);

private:
	Calibration m_cal;
	MS58xxLL m_ms58xx;
	AD5933LL m_ad5933;
	double m_last_temperature;
	double m_last_pressure;

	enum class CalibrationPoint : unsigned int {
		GAIN_FACTOR,
		CA,
		CB,
		CC
	};

	double read_calibrated_conductivity();
};
