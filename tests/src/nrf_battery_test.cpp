#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bsp.hpp"
#include "nrf_battery_mon.hpp"
#include "nrfx_saadc.h"


TEST_GROUP(NrfBattery)
{
	void setup() {
	}

	void teardown() {
	}
};


TEST(NrfBattery, CheckBatteryMon_CHEM_S18650_2600) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_S18650_2600);
	batt_mon.start();
	SAADC::set_adc_value(5783);
	CHECK_EQUAL(3993, batt_mon.get_voltage());
	CHECK_EQUAL(77, batt_mon.get_level());
	SAADC::set_adc_value(6783);
	CHECK_EQUAL(4684, batt_mon.get_voltage());
	CHECK_EQUAL(100, batt_mon.get_level());
	SAADC::set_adc_value(1000);
	CHECK_EQUAL(690, batt_mon.get_voltage());
	CHECK_EQUAL(0, batt_mon.get_level());
	batt_mon.stop();
}

TEST(NrfBattery, CheckBatteryMon_BATT_CHEM_CGR18650_2250) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_CGR18650_2250);
	batt_mon.start();
	SAADC::set_adc_value(5783);
	CHECK_EQUAL(3993, batt_mon.get_voltage());
	CHECK_EQUAL(83, batt_mon.get_level());
	SAADC::set_adc_value(6783);
	CHECK_EQUAL(4684, batt_mon.get_voltage());
	CHECK_EQUAL(100, batt_mon.get_level());
	SAADC::set_adc_value(1000);
	CHECK_EQUAL(690, batt_mon.get_voltage());
	CHECK_EQUAL(0, batt_mon.get_level());
	batt_mon.stop();
}


TEST(NrfBattery, CheckBatteryMon_BATT_BATT_CHEM_NCR18650_3100_3400) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_NCR18650_3100_3400);
	batt_mon.start();
	SAADC::set_adc_value(5783);
	CHECK_EQUAL(3993, batt_mon.get_voltage());
	CHECK_EQUAL(81, batt_mon.get_level());
	SAADC::set_adc_value(6783);
	CHECK_EQUAL(4684, batt_mon.get_voltage());
	CHECK_EQUAL(100, batt_mon.get_level());
	SAADC::set_adc_value(1000);
	CHECK_EQUAL(690, batt_mon.get_voltage());
	CHECK_EQUAL(0, batt_mon.get_level());
	batt_mon.stop();
}
