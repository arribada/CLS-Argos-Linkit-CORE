#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "gnss_detector_service.hpp"
#include "mock_m8q.hpp"
#include "fake_rtc.hpp"
#include "fake_config_store.hpp"
#include "fake_logger.hpp"
#include "fake_timer.hpp"
#include "fake_battery_mon.hpp"
#include "dte_protocol.hpp"
#include "mock_comparators.hpp"


extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC *rtc;
extern BatteryMonitor *battery_monitor;

#define FIRST_AQPERIOD		(30)


TEST_GROUP(GNSSDetector)
{
	FakeConfigurationStore *fake_config_store;
	FakeRTC *fake_rtc;
	FakeTimer *fake_timer;
	FakeLog *fake_log;
	MockM8Q *mock_m8q;
    MockStdFunctionVoidComparator m_comparator_std_func;
    MockGPSNavSettingsComparator  m_comparator_nav;

	void setup() {
	    mock().installComparator("std::function<void()>", m_comparator_std_func);
	    mock().installComparator("const GPSNavSettings&", m_comparator_nav);
		fake_log = new FakeLog("GPS");
		mock_m8q = new MockM8Q;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
		fake_timer = new FakeTimer;
		system_timer = fake_timer;
		system_scheduler = new Scheduler(system_timer);
		m_current_ms = 0;

		// Initialise configuration store (applies defaults)
		configuration_store->init();
	}

	void teardown() {
		delete system_scheduler;
		delete fake_timer;
		delete fake_rtc;
		delete fake_config_store;
		delete mock_m8q;
		delete fake_log;
	}

	void increment_time_ms(uint64_t ms)
	{
		while (ms)
		{
			m_current_ms++;
			if (m_current_ms % 1000 == 0)
				fake_rtc->incrementtime(1);
			fake_timer->increment_counter(1);

			system_scheduler->run();

			ms--;
		}
	}

	void increment_time_s(uint64_t s)
	{
		increment_time_ms(s * 1000);
	}

	void increment_time_min(uint64_t min)
	{
		increment_time_ms(min * 60 * 1000);
	}

	void notify_underwater_state(bool state) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_LOG_UPDATED;
		e.event_data = state;
		e.event_source = ServiceIdentifier::UW_SENSOR;
		e.event_originator_unique_id = 0x12345678;
		ServiceManager::notify_peer_event(e);
	}

	uint64_t m_current_ms;
};


TEST(GNSSDetector, GNSSDetectorDisabled)
{
	bool en = false;
	BaseUnderwaterDetectSource src = BaseUnderwaterDetectSource::GNSS;
	fake_config_store->write_param(ParamID::UNDERWATER_EN, en);
	fake_config_store->write_param(ParamID::UNDERWATER_DETECT_SOURCE, src);

	GNSSDetectorService s(*mock_m8q);
	s.start();

	increment_time_s(1);
}

TEST(GNSSDetector, GNSSDetectorEnabledAndSurfacedThenSubmerged)
{
	unsigned int num_callbacks = 0;
	bool switch_state = false;
	bool en = true;
	BaseUnderwaterDetectSource src = BaseUnderwaterDetectSource::GNSS;
	unsigned int max_samples = 10;
	unsigned int min_dry_samples = 1;
	unsigned int dry_schedule = 60;
	unsigned int wet_schedule = 60;
	unsigned int threshold = 3;

	fake_config_store->write_param(ParamID::UNDERWATER_EN, en);
	fake_config_store->write_param(ParamID::UNDERWATER_DETECT_SOURCE, src);
	fake_config_store->write_param(ParamID::UW_GNSS_DETECT_THRESH, threshold);
	fake_config_store->write_param(ParamID::UW_GNSS_MAX_SAMPLES, max_samples);
	fake_config_store->write_param(ParamID::UW_GNSS_MIN_DRY_SAMPLES, min_dry_samples);
	fake_config_store->write_param(ParamID::UW_GNSS_DRY_SAMPLING, dry_schedule);
	fake_config_store->write_param(ParamID::UW_GNSS_WET_SAMPLING, wet_schedule);

	GNSSDetectorService s(*mock_m8q);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			num_callbacks++;
		}
	});

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);
	mock().expectOneCall("power_off").onObject(mock_m8q).ignoreOtherParameters();
	mock_m8q->notify_sat_report(threshold);
	CHECK_EQUAL(1, num_callbacks);
	CHECK_FALSE(switch_state);

	num_callbacks = 0;
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(dry_schedule);

	for (unsigned int i = 0; i < max_samples; i++)
		mock_m8q->notify_sat_report(0);

	mock().expectOneCall("power_off").onObject(mock_m8q).ignoreOtherParameters();
	mock_m8q->notify_max_sat_samples();
	CHECK_EQUAL(1, num_callbacks);
	CHECK_TRUE(switch_state);
}


TEST(GNSSDetector, GNSSDetectorEnabledWithSWSDetectSurfacingOnly)
{
	unsigned int num_callbacks = 0;
	bool switch_state = false;
	bool en = true;
	BaseUnderwaterDetectSource src = BaseUnderwaterDetectSource::SWS_GNSS;
	unsigned int max_samples = 10;
	unsigned int min_dry_samples = 1;
	unsigned int dry_schedule = 60;
	unsigned int wet_schedule = 60;
	unsigned int threshold = 3;

	fake_config_store->write_param(ParamID::UNDERWATER_EN, en);
	fake_config_store->write_param(ParamID::UNDERWATER_DETECT_SOURCE, src);
	fake_config_store->write_param(ParamID::UW_GNSS_DETECT_THRESH, threshold);
	fake_config_store->write_param(ParamID::UW_GNSS_MAX_SAMPLES, max_samples);
	fake_config_store->write_param(ParamID::UW_GNSS_MIN_DRY_SAMPLES, min_dry_samples);
	fake_config_store->write_param(ParamID::UW_GNSS_DRY_SAMPLING, dry_schedule);
	fake_config_store->write_param(ParamID::UW_GNSS_WET_SAMPLING, wet_schedule);

	GNSSDetectorService s(*mock_m8q);
	s.start([&switch_state, &num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			switch_state = std::get<bool>(event.event_data);
			num_callbacks++;
		}
	});

	// Expect surfaced event when no existing state available
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);
	mock().expectOneCall("power_off").onObject(mock_m8q).ignoreOtherParameters();
	mock_m8q->notify_sat_report(threshold);
	CHECK_EQUAL(1, num_callbacks);
	CHECK_FALSE(switch_state);

	// Do not expect submerge event from GNSS when SWS is also used
	num_callbacks = 0;
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(dry_schedule);
	for (unsigned int i = 0; i < max_samples; i++)
		mock_m8q->notify_sat_report(0);
	mock().expectOneCall("power_off").onObject(mock_m8q).ignoreOtherParameters();
	mock_m8q->notify_max_sat_samples();

	CHECK_EQUAL(0, num_callbacks);
	CHECK_EQUAL(0, num_callbacks);
	CHECK_FALSE(switch_state);

	// Notify submerged state from another source
	notify_underwater_state(true);

	// Expect surface event from GNSS once more
	num_callbacks = 0;
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(dry_schedule);
	mock().expectOneCall("power_off").onObject(mock_m8q).ignoreOtherParameters();
	mock_m8q->notify_sat_report(threshold);
	CHECK_EQUAL(1, num_callbacks);
	CHECK_FALSE(switch_state);
}
