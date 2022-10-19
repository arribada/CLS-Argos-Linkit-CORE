#pragma once

#include "ms58xx.hpp"
#include "sensor.hpp"
#include "ad5933.hpp"

class CDT : public Sensor {
public:
	CDT(MS58xxLL& ms58xx, AD5933LL& ad5933);
	void calibration_write(const double, const unsigned int) override;
	void calibration_save(bool force) override;
	double read(unsigned int offset);

private:
	Calibration m_cal;
	MS58xxLL& m_ms58xx;
	AD5933LL& m_ad5933;
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
