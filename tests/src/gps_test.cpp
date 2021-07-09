#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

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
extern ServiceScheduler *location_scheduler;
extern Scheduler *system_scheduler;
extern Logger *sensor_log;
extern RTC *rtc;
extern BatteryMonitor *battery_monitor;

#define FIRST_AQPERIOD		(30)


TEST_GROUP(GpsScheduler)
{
	FakeBatteryMonitor *fake_battery_mon;
	FakeConfigurationStore *fake_config_store;
	GPSScheduler *gps_sched;
	MockM8Q *mock_m8q;
	FakeRTC *fake_rtc;
	FakeTimer *fake_timer;
	FakeLog *fake_log;
    MockStdFunctionVoidComparator m_comparator_std_func;
    MockGPSNavSettingsComparator  m_comparator_nav;

	void setup() {
	    mock().installComparator("std::function<void()>", m_comparator_std_func);
	    mock().installComparator("const GPSNavSettings&", m_comparator_nav);
		mock_m8q = new MockM8Q;
		gps_sched = mock_m8q;
		location_scheduler = gps_sched;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		fake_battery_mon = new FakeBatteryMonitor;
		battery_monitor = fake_battery_mon;
		fake_log = new FakeLog;
		sensor_log = fake_log;
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
		delete fake_log;
		delete fake_config_store;
		delete mock_m8q;
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


TEST(GpsScheduler, GNSSDisabled)
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

	location_scheduler->start();

	increment_time_min(60);
}

TEST(GpsScheduler, GNSSEnabled10MinutesNoFixB)
{
	int iterations = 3;
	unsigned int period_minutes = 10;

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
	const GPSNavSettings nav_settings = { fix_mode, dyn_model };

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).withParameterOfType("const GPSNavSettings&", "nav_settings", &nav_settings).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled15MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 15;

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
	BaseGNSSFixMode fix_mode = BaseGNSSFixMode::FIX_3D;
	fake_config_store->write_param(ParamID::GNSS_FIX_MODE, fix_mode);
	BaseGNSSDynModel dyn_model = BaseGNSSDynModel::AUTOMOTIVE;
	fake_config_store->write_param(ParamID::GNSS_DYN_MODEL, dyn_model);
	const GPSNavSettings nav_settings = { fix_mode, dyn_model };

	fake_rtc->settime(1580083200); // 27/01/2020 00:00:00

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).withParameterOfType("const GPSNavSettings&", "nav_settings", &nav_settings).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled30MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 30;

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

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled60MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 60;

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

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled120MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 120;

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

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled360MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 360;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 360*60;
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

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled720MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 720;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 720*60;
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

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled1440MinutesNoFix)
{
	int iterations = 3;
	unsigned int period_minutes = 1440;

	bool lb_en = false;
	unsigned int lb_threshold = 0U;
	bool gnss_en = true;
	unsigned int dloc_arg_nom = 1440*60;
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

	location_scheduler->start();

	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s((period_minutes * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabled10MinutesNoFixOffSchedule)
{
	int iterations = 3;
	unsigned int period_minutes = 10;

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

	location_scheduler->start();

	// We're expecting the device to turn on at 27/01/2020 00:05:30 for first attempt
	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(((period_minutes - 5) * 60) - FIRST_AQPERIOD - 1);

	for (int i = 0; i < iterations; ++i)
	{
		mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
		increment_time_s(1);
		mock().expectOneCall("power_off").onObject(mock_m8q);
		increment_time_s((period_minutes * 60) - 1);
	}
}

TEST(GpsScheduler, GNSSEnabledColdStartTimeoutCheck)
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

	location_scheduler->start();

	// We're expecting the device to turn on at 27/01/2020 00:00:30
	increment_time_s(FIRST_AQPERIOD - 1);

	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_s(1);

	// Should not power down yet
	increment_time_s(gnss_acq_timeout);

	// Should now power down
	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(gnss_acq_timeout_cold_start - gnss_acq_timeout);

	// Repeat to make sure cold start is still used
	mock().expectOneCall("power_on").onObject(mock_m8q).ignoreOtherParameters();
	increment_time_min(8);

	// Should not power down yet
	increment_time_s(gnss_acq_timeout);

	// Should now power down
	mock().expectOneCall("power_off").onObject(mock_m8q);
	increment_time_s(gnss_acq_timeout_cold_start - gnss_acq_timeout);
}

TEST(GpsScheduler, GNSSEnabledNominalTimeoutAfterFirstFix)
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

	location_scheduler->start();

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

TEST(GpsScheduler, GNSSEnabledWithHdopAndHaccFilter)
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

	location_scheduler->start();

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
