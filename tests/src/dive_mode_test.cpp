#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "dive_mode_service.hpp"
#include "fake_config_store.hpp"
#include "fake_timer.hpp"
#include "fake_irq.hpp"
#include "fake_switch.hpp"
#include "scheduler.hpp"


extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;


TEST_GROUP(DiveMode)
{
	FakeConfigurationStore *fake_config_store;
	FakeTimer *fake_timer;
	FakeSwitch fake_switch;
	FakeIRQ fake_irq;

	void setup() {
		fake_timer = new FakeTimer;
		system_timer = fake_timer;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		system_scheduler = new Scheduler(system_timer);
		configuration_store->init();
		system_timer->start();
		fake_switch.resume();
	}

	void teardown() {
		delete system_scheduler;
		delete fake_timer;
		delete fake_config_store;
	}

	void notify_underwater_state(bool state) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_LOG_UPDATED,
		e.event_data = state,
		e.event_source = ServiceIdentifier::UW_SENSOR;
		ServiceManager::notify_peer_event(e);
	}

	void advance_time(unsigned int ms) {
		fake_timer->increment_counter(ms);
		system_scheduler->run();
	}
};


TEST(DiveMode, DiveModeEngagedAndDisengagedByWCHG)
{
	unsigned int start_period = 10;
	bool dive_mode_en = true;

	configuration_store->write_param(ParamID::UW_DIVE_MODE_ENABLE, dive_mode_en);
	configuration_store->write_param(ParamID::UW_DIVE_MODE_START_TIME, start_period);

	DiveModeService s(fake_switch, fake_irq);
	s.start();
	notify_underwater_state(true);
	CHECK_EQUAL(1000 * start_period, s.get_last_schedule());
	advance_time(s.get_last_schedule());
	CHECK_TRUE(fake_switch.is_paused());
	fake_irq.invoke();
	CHECK_FALSE(fake_switch.is_paused());
	s.stop();
	CHECK_FALSE(fake_switch.is_paused());
}


TEST(DiveMode, DiveModeEngagedAndDisengagedBySurfacing)
{
	unsigned int start_period = 10;
	bool dive_mode_en = true;

	configuration_store->write_param(ParamID::UW_DIVE_MODE_ENABLE, dive_mode_en);
	configuration_store->write_param(ParamID::UW_DIVE_MODE_START_TIME, start_period);

	DiveModeService s(fake_switch, fake_irq);
	s.start();
	notify_underwater_state(true);
	CHECK_EQUAL(1000 * start_period, s.get_last_schedule());
	advance_time(s.get_last_schedule());
	CHECK_TRUE(fake_switch.is_paused());
	notify_underwater_state(false);
	CHECK_FALSE(fake_switch.is_paused());
	s.stop();
	CHECK_FALSE(fake_switch.is_paused());
}

TEST(DiveMode, DiveModeHasNoEffectWhenDisabled)
{
	unsigned int start_period = 10;
	bool dive_mode_en = false;

	configuration_store->write_param(ParamID::UW_DIVE_MODE_ENABLE, dive_mode_en);
	configuration_store->write_param(ParamID::UW_DIVE_MODE_START_TIME, start_period);

	DiveModeService s(fake_switch, fake_irq);
	s.start();
	notify_underwater_state(true);
	CHECK_EQUAL(Service::SCHEDULE_DISABLED, s.get_last_schedule());
	CHECK_FALSE(fake_switch.is_paused());
	fake_irq.invoke();
	CHECK_FALSE(fake_switch.is_paused());
	s.stop();
	CHECK_FALSE(fake_switch.is_paused());
}

TEST(DiveMode, DiveModeDisengagedIfServiceStopped)
{
	unsigned int start_period = 10;
	bool dive_mode_en = true;

	configuration_store->write_param(ParamID::UW_DIVE_MODE_ENABLE, dive_mode_en);
	configuration_store->write_param(ParamID::UW_DIVE_MODE_START_TIME, start_period);

	DiveModeService s(fake_switch, fake_irq);
	s.start();
	notify_underwater_state(true);
	CHECK_EQUAL(1000 * start_period, s.get_last_schedule());
	advance_time(s.get_last_schedule());
	CHECK_TRUE(fake_switch.is_paused());
	s.stop();
	CHECK_FALSE(fake_switch.is_paused());
}


TEST(DiveMode, DiveModeRunsMultipleTimes)
{
	unsigned int start_period = 10;
	bool dive_mode_en = true;

	configuration_store->write_param(ParamID::UW_DIVE_MODE_ENABLE, dive_mode_en);
	configuration_store->write_param(ParamID::UW_DIVE_MODE_START_TIME, start_period);

	DiveModeService s(fake_switch, fake_irq);
	s.start();

	for (unsigned int i = 0; i < 10; i++) {
		notify_underwater_state(true);
		CHECK_EQUAL(1000 * start_period, s.get_last_schedule());
		advance_time(s.get_last_schedule());
		CHECK_TRUE(fake_switch.is_paused());
		notify_underwater_state(false);
		CHECK_FALSE(fake_switch.is_paused());
	}

	s.stop();
	CHECK_FALSE(fake_switch.is_paused());
}
