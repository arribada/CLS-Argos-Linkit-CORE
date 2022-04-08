#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>

#include "sws.hpp"
#include "bsp.hpp"
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
		configuration_store->init();
	}

	void teardown() {
		delete system_scheduler;
		delete linux_timer;
		delete fake_config_store;
	}
};

TEST(SWS, UnderwaterEvent)
{
	SWS s(1);   // Use 1 msec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 1;
	bool underwater_en = true;

	configuration_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			num_callbacks++;
		}
	});

	for (unsigned int i = 0; i < 5; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("delay_ms").withUnsignedIntParameter("ms", 1);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_RX).andReturnValue(1);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_TRUE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, SurfacedEvent)
{
	SWS s(1);   // Use 1 msec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 1;
	bool underwater_en = true;

	configuration_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			num_callbacks++;
		}
	});

	for (unsigned int i = 0; i < 1; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("delay_ms").withUnsignedIntParameter("ms", 1);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_RX).andReturnValue(0);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_FALSE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, SchedulingPeriodSurfaced)
{
	SWS s(1);   // Use 1 msec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 2;
	unsigned int under_period = 1;
	bool underwater_en = true;

	configuration_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			num_callbacks++;
		}
	});

	// Sampling should happen every 2 seconds when surfaced
	for (unsigned int i = 0; i < 5; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("delay_ms").withUnsignedIntParameter("ms", 1);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_RX).andReturnValue(0);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_FALSE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, SchedulingPeriodUnderwater)
{
	SWS s(1);   // Use 1 msec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 2;
	bool underwater_en = true;

	configuration_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			DEBUG_TRACE("state: %u", switch_state);
			num_callbacks++;
		}
	});

	for (unsigned int i = 0; i < 10; i++) {
		mock().expectOneCall("set").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("delay_ms").withUnsignedIntParameter("ms", 1);
		mock().expectOneCall("clear").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_SEND);
		mock().expectOneCall("value").withUnsignedIntParameter("pin", BSP::GPIO::GPIO_SLOW_SWS_RX).andReturnValue(1);
		DEBUG_TRACE("t=%lu", linux_timer->get_counter());
		while (!system_scheduler->run());
	}

	CHECK_TRUE(switch_state);
	CHECK_EQUAL(1, num_callbacks);

	s.stop();
}

TEST(SWS, UnderwaterModeDisabled)
{
	SWS s(1);   // Use 1 msec scheduling units
	bool switch_state = false;
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int surf_period = 1;
	unsigned int under_period = 2;
	bool underwater_en = false;

	configuration_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	configuration_store->write_param(ParamID::SAMPLING_UNDER_FREQ, under_period);
	configuration_store->write_param(ParamID::SAMPLING_SURF_FREQ, surf_period);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			num_callbacks++;
		}
	});

	CHECK_FALSE(system_scheduler->is_any_task_scheduled());

	s.stop();
}
