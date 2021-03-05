#ifndef __NRF_BATTERY_MON_HPP_
#define __NRF_BATTERY_MON_HPP_

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

public:

	NrfBatteryMonitor(uint8_t adc_channel, BatteryChemistry chem = BATT_CHEM_NCR18650_3100_3400);
	void start() override {};
	void stop() override {};
	uint16_t get_voltage() override;
	uint8_t get_level() override;
};

#endif // __NRF_BATTERY_MON_HPP_
