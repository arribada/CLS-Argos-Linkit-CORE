#pragma once

#include "sensor.hpp"
#include "calibration.hpp"
#include "ad5933.hpp"
#include "ms5803.hpp"

class CDT : public Sensor {
public:
	CDT();
	void calibrate(double, unsigned int) override;
	double read(unsigned int offset);

private:
	Calibration m_cal;
	MS5803LL m_ms5803;
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
