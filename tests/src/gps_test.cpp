#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "gps_service.hpp"
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


TEST_GROUP(GPSService)
{
	FakeBatteryMonitor *fake_battery_mon;
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
		fake_battery_mon = new FakeBatteryMonitor;
		battery_monitor = fake_battery_mon;
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

	uint64_t m_current_ms;
};


TEST(GPSService, GNSSDisabled)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = false;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 0;
	unsigned int gnss_acq_timeout_cold_start = 0;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	GPSService s(*mock_m8q, fake_log);
	s.start();

	increment_time_min(60);
}

TEST(GPSService, GNSSEnabled10MinutesDloc)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	unsigned int offset = FIRST_AQPERIOD;

	int iterations = 3;
	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(dloc_arg_nom - offset);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s(gnss_acq_timeout);
		offset = gnss_acq_timeout;
	}
}

TEST(GPSService, GNSSEnabled15MinutesDloc)
{
	int iterations = 3;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 15*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	unsigned int offset = FIRST_AQPERIOD;

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(dloc_arg_nom - offset);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s(gnss_acq_timeout);
		offset = gnss_acq_timeout;
	}
}

TEST(GPSService, GNSSEnabled30MinutesDloc)
{
	int iterations = 3;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 30*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	unsigned int offset = FIRST_AQPERIOD;

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(dloc_arg_nom - offset);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s(gnss_acq_timeout);
		offset = gnss_acq_timeout;
	}
}

TEST(GPSService, GNSSEnabled60MinutesDloc)
{
	int iterations = 3;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 60*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	unsigned int offset = FIRST_AQPERIOD;

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(dloc_arg_nom - offset);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s(gnss_acq_timeout);
		offset = gnss_acq_timeout;
	}
}

TEST(GPSService, GNSSEnabled120MinutesDloc)
{
	int iterations = 3;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 120*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	unsigned int offset = FIRST_AQPERIOD;

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(dloc_arg_nom - offset);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s(gnss_acq_timeout);
		offset = gnss_acq_timeout;
	}
}

TEST(GPSService, GNSSEnabled10MinutesDlocUTCOffset)
{
	int iterations = 3;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083500); // 27/01/2020 00:05:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	unsigned int offset = FIRST_AQPERIOD + (60*5);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(dloc_arg_nom - offset);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s(gnss_acq_timeout);
		offset = gnss_acq_timeout;
	}
}

TEST(GPSService, GNSSEnabledColdStartTimeoutAndRetryCheck)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 120;
	unsigned int gnss_cold_start_retry_period = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::GNSS_COLD_START_RETRY_PERIOD, gnss_cold_start_retry_period);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	// We're expecting the device to turn on at 27/01/2020 00:00:30

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Should power down after cold start timeout
	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(gnss_acq_timeout_cold_start);

	// Repeat to make sure cold start repeats after retry period (on UTC 60 seconds)
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(gnss_cold_start_retry_period - FIRST_AQPERIOD);

	// Make a fix to exit cold start mode
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	// Should now power down
	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	// Next schedule should be back to dloc_arg_nom
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(dloc_arg_nom - (m_current_ms/1000) + 1);
}

TEST(GPSService, GNSSEnabledNominalTimeoutAfterFirstFix)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 120;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	// We're expecting the device to turn on at 27/01/2020 00:00:30
	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	// Should not power down yet
	increment_time_s(gnss_acq_timeout);

	// Send a dummy GNSS data event to mark the first fix as being made
	mock_m8q->notify_gnss_data(fake_rtc->gettime());

	printf("rtc = %llu\n", fake_rtc->gettime());

	// Should now power down
	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);

	printf("rtc = %llu\n", fake_rtc->gettime());

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(509);

	printf("rtc = %llu\n", fake_rtc->gettime());

	// Should now power down at nominal timeout
	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(gnss_acq_timeout);
	increment_time_ms(1);

	printf("rtc = %llu\n", fake_rtc->gettime());

}

TEST(GPSService, GNSSEnabledWithHdopAndHaccFilter)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 120;
	bool gnss_hdopfilt_en = true;
	unsigned int gnss_hdopfilt_thres = 5;
	bool gnss_haccfilt_en = true;
	unsigned int gnss_haccfilt_thres = 50;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::GNSS_HACCFILT_EN, gnss_haccfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HACCFILT_THR, gnss_haccfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	// We're expecting the device to turn on at 27/01/2020 00:00:30
	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	// Should not power down yet
	increment_time_s(gnss_acq_timeout);

	// HDOP and HACC not met
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 7, 100000);
	increment_time_s(1);

	// HACC not met
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 100000);
	increment_time_s(1);

	// Both met
	mock().expectOneCall("power_off").onObject(mock_m8q);
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 35000);
	increment_time_s(1);
}

TEST(GPSService, GNSSEnabledWithHdopAndHaccFilterAndNConsecutiveFixes)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 120;
	bool gnss_hdopfilt_en = true;
	unsigned int gnss_hdopfilt_thres = 5;
	bool gnss_haccfilt_en = true;
	unsigned int gnss_haccfilt_thres = 50;
	bool underwater_en = false;
	unsigned int consecutive_fixes = 3;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::GNSS_HACCFILT_EN, gnss_haccfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HACCFILT_THR, gnss_haccfilt_thres);
	fake_config_store->write_param(ParamID::GNSS_MIN_NUM_FIXES, consecutive_fixes);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	// We're expecting the device to turn on at 27/01/2020 00:00:30
	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	// Should not power down yet
	increment_time_s(gnss_acq_timeout);

	// HDOP and HACC not met
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 7, 100000);
	increment_time_s(1);

	// HACC not met
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 100000);
	increment_time_s(1);

	// Both met (1 fix)
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 35000);
	increment_time_s(1);

	// Both met (2 fixes)
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 35000);
	increment_time_s(1);

	// Should reset the counter
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 100000);
	increment_time_s(1);

	// Both met (1 fix)
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 35000);
	increment_time_s(1);

	// Both met (2 fixes)
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 35000);
	increment_time_s(1);

	// Both met (3 fixes)
	mock().expectOneCall("power_off").onObject(mock_m8q);
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10, 3, 35000);
	increment_time_s(1);
}


TEST(GPSService, GNSSInterruptedByUnderwaterEvent)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 120;
	bool gnss_hdopfilt_en = false;
	bool gnss_haccfilt_en = false;
	bool underwater_en = true;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HACCFILT_EN, gnss_haccfilt_en);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	// We're expecting the device to turn on at 27/01/2020 00:00:30
	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	// Now fire an underwater event before we get GPS lock
	mock().expectOneCall("power_off").onObject(mock_m8q);
	s.notify_underwater_state(true);
}


TEST(GPSService, GNSSIgnoredAfterUnderwaterEvent)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 10*60;
	unsigned int gnss_acq_timeout = 60;
	bool gnss_hdopfilt_en = false;
	bool gnss_haccfilt_en = false;
	bool underwater_en = true;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HACCFILT_EN, gnss_haccfilt_en);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	GPSService s(*mock_m8q, fake_log);
	s.start();

	// Now fire an underwater event before we schedule
	s.notify_underwater_state(true);

	// We're expecting the device to turn on at 27/01/2020 00:00:30 - remain off
	increment_time_s(FIRST_AQPERIOD);

	// Next schedule attempt will be at 00:01:00 - remain off
	increment_time_s(30);

	// Next schedule attempt will be at 00:02:00 - remain off
	increment_time_s(60);

	// Now fire a surfaced event - next time will power on
	s.notify_underwater_state(false);

	// Next schedule attempt will be at 00:03:00 - power on
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(60);
}


TEST(GPSService, GNSSNoPeriodicTriggerOnSurfaceEvent)
{
	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 0;
	unsigned int gnss_acq_timeout = 60;
	unsigned int gnss_acq_timeout_cold_start = 60;
	bool gnss_hdopfilt_en = false;
	unsigned int gnss_hdopfilt_thres = 0;
	bool underwater_en = true;
	bool trigger_surface_en = true;

	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::GNSS_ACQ_TIMEOUT, gnss_acq_timeout);
	fake_config_store->write_param(ParamID::GNSS_COLD_ACQ_TIMEOUT, gnss_acq_timeout_cold_start);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_EN, gnss_hdopfilt_en);
	fake_config_store->write_param(ParamID::GNSS_HDOPFILT_THR, gnss_hdopfilt_thres);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_2D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::SEA;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);
	fake_config_store->write_param(ParamID::GNSS_TRIGGER_ON_SURFACED, trigger_surface_en);

	fake_rtc->settime(0);

	GPSService s(*mock_m8q, fake_log);
	s.start();

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(FIRST_AQPERIOD);

	// Send dummy fix
	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1000);

	// Now fire a surfaced event - next time will power on
	s.notify_underwater_state(false);
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock_m8q->notify_gnss_data(fake_rtc->gettime(), 10, 10);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(1);
}
