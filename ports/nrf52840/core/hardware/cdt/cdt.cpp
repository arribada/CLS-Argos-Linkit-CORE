#include "cdt.hpp"
#include "debug.hpp"

CDT::CDT(unsigned int bus) : Sensor("CDT"), m_cal(Calibration("CDT")), m_ms58xx(MS58xxLL(bus, MS5837_ADDRESS, MS5837_VARIANT)),
	m_ad5933(AD5933LL(bus, AD5933_ADDRESS)) {
	DEBUG_TRACE("CDT::CDT");
}

double CDT::read(unsigned int offset) {
	switch(offset) {
	case 0:
		return read_calibrated_conductivity();
		break;
	case 1:
		m_ms58xx.read(m_last_temperature, m_last_pressure);
		return m_last_pressure;
		break;
	case 2:
		return m_last_temperature;
		break;
	default:
		return 0;
	}
}

void CDT::calibrate(double value, unsigned int offset) {
	if (0 == offset) {
		DEBUG_TRACE("CDT::calibrate: reset calibration");
		m_cal.reset();
	} else if (1 == offset) {
		double magnitude = m_ad5933.get_iq_magnitude_single_point(90000);
		double impedance_ref = value;
		double gain_factor = (1 / impedance_ref) / magnitude;
		DEBUG_TRACE("CDT::calibrate: measured gain_factor = %f", gain_factor);
		m_cal.write((unsigned int)CalibrationPoint::GAIN_FACTOR, gain_factor);
	} else if (2 == offset) {
		DEBUG_TRACE("CDT::calibrate: CA = %f", value);
		m_cal.write((unsigned int)CalibrationPoint::CA, value);
	} else if (3 == offset) {
		DEBUG_TRACE("CDT::calibrate: CB = %f", value);
		m_cal.write((unsigned int)CalibrationPoint::CB, value);
	} else if (4 == offset) {
		DEBUG_TRACE("CDT::calibrate: CC = %f", value);
		m_cal.write((unsigned int)CalibrationPoint::CC, value);
	} else if (5 == offset) {
		DEBUG_TRACE("CDT::calibrate: saving calibration");
		m_cal.save();
	}
}

double CDT::read_calibrated_conductivity() {
	double gain_factor, CA, CB, CC;
	try {
		gain_factor = m_cal.read((unsigned int)CalibrationPoint::GAIN_FACTOR);
	} catch (...) {
		DEBUG_TRACE("CDT::read_calibrated_conductivity: GF calibration missing, using default calibration");
		gain_factor = 10.95E-6;
	}

	try {
		CA = m_cal.read((unsigned int)CalibrationPoint::CA);
		CB = m_cal.read((unsigned int)CalibrationPoint::CB);
		CC = m_cal.read((unsigned int)CalibrationPoint::CC);
	} catch (...) {
		DEBUG_TRACE("CDT::read_calibrated_conductivity: CA/CB/CC calibration missing, using default calibration");
		CA = 0.0011;
		CB = 0.9095;
		CC = 0.4696;
	}

	double impedance = (1 / (gain_factor * m_ad5933.get_iq_magnitude_single_point(90000, 2)));
	double conduino_value = impedance * 1000;
	double conductivity = 1000 * (CA * conduino_value * conduino_value + CB * conduino_value + CC);

	DEBUG_TRACE("CDT::read_calibrated_conductivity: conductivity = %f", conductivity);

	return conductivity;
}
