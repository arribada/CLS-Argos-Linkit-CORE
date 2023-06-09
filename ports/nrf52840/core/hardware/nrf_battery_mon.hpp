#pragma once

#include "battery.hpp"


enum BatteryChemistry {
	BATT_CHEM_S18650_2600,
	BATT_CHEM_CGR18650_2250,
	BATT_CHEM_NCR18650_3100_3400
};

class NrfBatteryMonitor : public BatteryMonitor {
private:
	BatteryChemistry m_chem;
	uint8_t m_adc_channel;
	bool m_is_init;

	float sample_adc();
	uint16_t convert_voltage(float);
	uint8_t convert_level(uint16_t);
	void internal_update() override;

public:
	NrfBatteryMonitor(uint8_t adc_channel,
			BatteryChemistry chem = BATT_CHEM_NCR18650_3100_3400,
			uint16_t critical_voltage = 2800,
			uint8_t low_level = 10);
};
