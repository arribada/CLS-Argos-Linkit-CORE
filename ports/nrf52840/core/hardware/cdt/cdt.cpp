#include "cdt.hpp"
#include "debug.hpp"

CDT::CDT(MS58xxHardware& ms58xx, AD5933& ad5933) : Sensor("CDT"), m_cal(Calibration("CDT")), m_ms58xx(ms58xx), m_ad5933(ad5933) {
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

void CDT::calibration_write(const double value, const unsigned int offset) {
	if (0 == offset) {
		DEBUG_TRACE("CDT::calibrate: reset calibration");
		m_cal.reset();
	} else if (2 == offset) {
		DEBUG_TRACE("CDT::calibrate: CA = %lf", value);
		m_cal.write((unsigned int)CalibrationPoint::CA, value);
	} else if (3 == offset) {
		DEBUG_TRACE("CDT::calibrate: CB = %lf", value);
		m_cal.write((unsigned int)CalibrationPoint::CB, value);
	} else if (4 == offset) {
		DEBUG_TRACE("CDT::calibrate: CC = %lf", value);
		m_cal.write((unsigned int)CalibrationPoint::CC, value);
	} else if (5 == offset) {
		DEBUG_TRACE("CDT::calibrate: saving calibration");
		m_cal.save();
	} else if (6 == offset) {
		DEBUG_TRACE("CDT::calibrate: gain_factor = %lf", value);
		m_cal.write((unsigned int)CalibrationPoint::GAIN_FACTOR, value);
	} else if (7 == offset) {
		DEBUG_TRACE("CDT::calibrate: power on AD5933 using f=%u", (unsigned int)value);
		m_ad5933.start((unsigned int)value, VRange::V400MV_GAIN1X);
	} else if (8 == offset) {
		DEBUG_TRACE("CDT::calibrate: power off AD5933");
		m_ad5933.stop();
	}
}

void CDT::calibration_read(double& value, const unsigned int offset) {
	if (0 == offset) {
		DEBUG_TRACE("CDT::calibrate: read CA");
		value = m_cal.read((unsigned int)CalibrationPoint::CA);
	} else if (1 == offset) {
		DEBUG_TRACE("CDT::calibrate: read CB");
		value = m_cal.read((unsigned int)CalibrationPoint::CB);
	} else if (2 == offset) {
		DEBUG_TRACE("CDT::calibrate: read CC");
		value = m_cal.read((unsigned int)CalibrationPoint::CC);
	} else if (3 == offset) {
		DEBUG_TRACE("CDT::calibrate: read gain_factor");
		value = m_cal.read((unsigned int)CalibrationPoint::GAIN_FACTOR);
	} else if (4 == offset) {
		int16_t real, imag;
		// This will fetch I and Q samples so must be called first
		m_ad5933.get_real_imaginary(real, imag);
		DEBUG_TRACE("CDT::calibrate: read real value: %d,%d", (int)real, (int)imag);
		value = (double)real;
		m_last_imaginary = imag;
	} else if (5 == offset) {
		DEBUG_TRACE("CDT::calibrate: read imaginary value");
		value = (double)m_last_imaginary;
	} else if (6 == offset) {
		DEBUG_TRACE("CDT::calibrate: read impedence using calibrated gain");
		value = m_ad5933.get_impedence(1, m_cal.read((unsigned int)CalibrationPoint::GAIN_FACTOR));
	}
}

void CDT::calibration_save(bool force) {
	m_cal.save(force);
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

	m_ad5933.start(90000, VRange::V400MV_GAIN1X);
	double impedance = m_ad5933.get_impedence(2, gain_factor);
	m_ad5933.stop();

	double conduino_value = (1 / impedance) * 1000;
	double conductivity = 1000 * (CA * conduino_value * conduino_value + CB * conduino_value + CC);

	DEBUG_TRACE("CDT::read_calibrated_conductivity: conductivity = %lf", conductivity);

	return conductivity;
}