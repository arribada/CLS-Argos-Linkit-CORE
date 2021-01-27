#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>
#include "sws.hpp"
#include "fake_config_store.hpp"
#include "linux_timer.hpp"


extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;


TEST_GROUP(SWS)
{
	FakeConfigurationStore *fake_config_store;
	LinuxTimer *linux_timer;
	void setup() {
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		linux_timer = new LinuxTimer;
		system_timer = linux_timer;
		system_scheduler = new Scheduler(system_timer);
	}

	void teardown() {
		delete system_scheduler;
		delete linux_timer;
		delete fake_config_store;
	}
};

TEST(SWS, UnderwaterEvent)
{
	SWS s(1);   // Use 1 sec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 1;

	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](bool state) { switch_state = state; num_callbacks++; } );

	for (unsigned int i = 0; i < 5; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", 1).andReturnValue(1);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_TRUE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, SurfacedEvent)
{
	SWS s(1);   // Use 1 sec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 1;

	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](bool state) { switch_state = state; num_callbacks++; } );

	for (unsigned int i = 0; i < 1; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", 1).andReturnValue(0);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_FALSE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, SchedulingPeriodSurfaced)
{
	SWS s(1);   // Use 1 sec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 2;
	unsigned int under_period = 1;

	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](bool state) { switch_state = state; num_callbacks++; } );

	// Sampling should happen every 2 seconds when surfaced
	for (unsigned int i = 0; i < 5; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", 1).andReturnValue(0);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_FALSE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, SchedulingPeriodUnderwater)
{
	SWS s(1);   // Use 1 sec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 2;

	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](bool state) { switch_state = state; num_callbacks++; } );

	for (unsigned int i = 0; i < 10; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", 0);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", 1).andReturnValue(1);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_TRUE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}
