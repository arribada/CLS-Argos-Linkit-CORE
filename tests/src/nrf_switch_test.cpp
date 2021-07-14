#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "nrfx_gpiote.h"
#include "bsp.hpp"
#include "fake_timer.hpp"
#include "scheduler.hpp"
#include "gpio.hpp"
#include "nrf_switch.hpp"
#include "nrfx_gpiote.h"


extern Timer *system_timer;


namespace BSP
{
	const GPIO_InitTypeDefAndInst_t GPIO_Inits[GPIO_TOTAL_NUMBER] = {};
};


TEST_GROUP(NrfSwitch)
{
	FakeTimer *fake_timer;
	void setup() {
		fake_timer = new FakeTimer;
		system_timer = fake_timer;
	}

	void teardown() {
		delete fake_timer;
	}
};


TEST(NrfSwitch, SwitchTriggeringCallbackNoHysteresis) {
	NrfSwitch *sw = new NrfSwitch(BSP::GPIO::GPIO_SWITCH, 0);
	unsigned int trigger_counter = 0;
	bool actual_state = false;

	system_timer->start();

	sw->start([&trigger_counter, &actual_state](bool state){ trigger_counter++; actual_state = state; });

	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(1);
	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);
	fake_timer->increment_counter(1);

	CHECK_EQUAL(1, trigger_counter);
	CHECK_TRUE(actual_state);

	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(0);
	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);
	fake_timer->increment_counter(1);

	CHECK_EQUAL(2, trigger_counter);
	CHECK_FALSE(actual_state);

	sw->stop();

	delete sw;
}


TEST(NrfSwitch, SwitchTriggeringCallbackWithHysteresis) {
	NrfSwitch *sw = new NrfSwitch(BSP::GPIO::GPIO_SWITCH, 1000);
	unsigned int trigger_counter = 0;
	bool actual_state = false;

	system_timer->start();

	sw->start([&trigger_counter, &actual_state](bool state){ trigger_counter++; actual_state = state; });

	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(1);
	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(0);
	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(1);

	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);  // 0->1
	fake_timer->increment_counter(100);
	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);  // 1->0
	fake_timer->increment_counter(100);
	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);  // 0->1
	fake_timer->increment_counter(1000);

	CHECK_EQUAL(1, trigger_counter);
	CHECK_TRUE(actual_state);

	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(0);
	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(1);
	mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SWITCH).andReturnValue(0);

	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);  // 1->0
	fake_timer->increment_counter(100);
	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);  // 0->1
	fake_timer->increment_counter(100);
	GPIOTE::trigger(BSP::GPIO::GPIO_SWITCH, NRF_GPIOTE_POLARITY_TOGGLE);  // 1->0
	fake_timer->increment_counter(1000);

	CHECK_EQUAL(2, trigger_counter);
	CHECK_FALSE(actual_state);

	sw->stop();

	delete sw;
}
