#pragma once

#include "ms58xx.hpp"
#include "sensor.hpp"
#include "ad5933.hpp"

class CDT : public Sensor {
public:
	CDT(MS58xxHardware& ms58xx, AD5933& ad5933);
	void calibration_write(const double, const unsigned int) override;
	void calibration_save(bool force) override;
	void calibration_read(double &value, const unsigned int) override;
	double read(unsigned int offset) override;

private:
	Calibration m_cal;
	MS58xxHardware& m_ms58xx;
	AD5933& m_ad5933;
	double m_last_temperature;
	double m_last_pressure;
	int16_t m_last_imaginary;

	enum class CalibrationPoint : unsigned int {
		GAIN_FACTOR,
		CA,
		CB,
		CC
	};

	double read_calibrated_conductivity();
};