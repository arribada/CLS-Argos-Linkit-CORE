#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bsp.hpp"
#include "nrf_battery_mon.hpp"
#include "nrfx_saadc.h"

namespace BSP
{
	const ADC_InitTypeDefAndInst_t ADC_Inits = {};
};

class BatteryCriticalHandler : public BatteryMonitorEventListener {
private:
	BatteryMonitor& m_mon;
public:
	BatteryCriticalHandler(BatteryMonitor& m) : m_mon(m) {
		m.subscribe(*this);
	}
	~BatteryCriticalHandler() {
		m_mon.unsubscribe(*this);
	}
	void react(BatteryMonitorEventVoltageCritical const &) override {
		mock().actualCall("BatteryMonitorEventVoltageCritical").onObject(this);
	}
};


TEST_GROUP(NrfBattery)
{
	void setup() {
	}

	void teardown() {
	}
};


TEST(NrfBattery, CheckBatteryMon_CHEM_S18650_2600) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_S18650_2600);
	SAADC::set_adc_value(9088);
	batt_mon.update();
	CHECK_EQUAL(3993, batt_mon.get_voltage());
	CHECK_EQUAL(77, batt_mon.get_level());
	SAADC::set_adc_value(10660);
	batt_mon.update();
	CHECK_EQUAL(4684, batt_mon.get_voltage());
	CHECK_EQUAL(100, batt_mon.get_level());
	SAADC::set_adc_value(1572);
	batt_mon.update();
	CHECK_EQUAL(690, batt_mon.get_voltage());
	CHECK_EQUAL(0, batt_mon.get_level());
}

TEST(NrfBattery, CheckBatteryMon_BATT_CHEM_CGR18650_2250) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_CGR18650_2250);
	SAADC::set_adc_value(9088);
	batt_mon.update();
	CHECK_EQUAL(3993, batt_mon.get_voltage());
	CHECK_EQUAL(83, batt_mon.get_level());
	SAADC::set_adc_value(10660);
	batt_mon.update();
	CHECK_EQUAL(4684, batt_mon.get_voltage());
	CHECK_EQUAL(100, batt_mon.get_level());
	SAADC::set_adc_value(1572);
	batt_mon.update();
	CHECK_EQUAL(690, batt_mon.get_voltage());
	CHECK_EQUAL(0, batt_mon.get_level());
}


TEST(NrfBattery, CheckBatteryMon_BATT_BATT_CHEM_NCR18650_3100_3400) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_NCR18650_3100_3400);
	SAADC::set_adc_value(9088);
	batt_mon.update();
	CHECK_EQUAL(3993, batt_mon.get_voltage());
	CHECK_EQUAL(81, batt_mon.get_level());
	SAADC::set_adc_value(10660);
	batt_mon.update();
	CHECK_EQUAL(4684, batt_mon.get_voltage());
	CHECK_EQUAL(100, batt_mon.get_level());
	SAADC::set_adc_value(1572);
	batt_mon.update();
	CHECK_EQUAL(690, batt_mon.get_voltage());
	CHECK_EQUAL(0, batt_mon.get_level());
}

TEST(NrfBattery, CheckIsBatteryLevelLowIndication) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_NCR18650_3100_3400, 0, 10);
	SAADC::set_adc_value(9088);
	batt_mon.update();
	CHECK_FALSE(batt_mon.is_battery_low());
	SAADC::set_adc_value(8000); // 7%
	batt_mon.update();
	CHECK_TRUE(batt_mon.is_battery_low());
	SAADC::set_adc_value(8100); // 11% - remain low battery state
	batt_mon.update();
	CHECK_TRUE(batt_mon.is_battery_low());
	SAADC::set_adc_value(8200); // 15% - exit low battery state
	batt_mon.update();
	CHECK_FALSE(batt_mon.is_battery_low());
	SAADC::set_adc_value(8000); // 7%
	batt_mon.update();
	CHECK_TRUE(batt_mon.is_battery_low());
}

TEST(NrfBattery, CheckIsBatteryCriticalIndication) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_NCR18650_3100_3400, 2500, 10);
	SAADC::set_adc_value(9088); // 4000 mV
	batt_mon.update();
	CHECK_FALSE(batt_mon.is_battery_critical());
	SAADC::set_adc_value(4088); // 1796 mV
	batt_mon.update();
	CHECK_TRUE(batt_mon.is_battery_critical());
	SAADC::set_adc_value(5088); // 2235 mV - remain critical battery state
	batt_mon.update();
	CHECK_TRUE(batt_mon.is_battery_critical());
	SAADC::set_adc_value(6500); // 2856 mV - exit critical battery state
	batt_mon.update();
	CHECK_FALSE(batt_mon.is_battery_critical());
	SAADC::set_adc_value(5088); // 2235 mV
	batt_mon.update();
	CHECK_TRUE(batt_mon.is_battery_critical());
}

TEST(NrfBattery, CheckIsBatteryCriticalEvents) {
	NrfBatteryMonitor batt_mon(BSP::ADC::ADC_CHANNEL_0, BATT_CHEM_NCR18650_3100_3400, 2500, 10);
	BatteryCriticalHandler event_handler(batt_mon);
	SAADC::set_adc_value(9088); // 4000 mV
	batt_mon.update();
	SAADC::set_adc_value(4088); // 1796 mV
	mock().expectOneCall("BatteryMonitorEventVoltageCritical").onObject(&event_handler);
	batt_mon.update();
	SAADC::set_adc_value(5088); // 2235 mV - remain critical battery state
	batt_mon.update();
	SAADC::set_adc_value(6500); // 2856 mV - exit critical battery state
	batt_mon.update();
	SAADC::set_adc_value(5088); // 2235 mV
	mock().expectOneCall("BatteryMonitorEventVoltageCritical").onObject(&event_handler);
	batt_mon.update();
}
