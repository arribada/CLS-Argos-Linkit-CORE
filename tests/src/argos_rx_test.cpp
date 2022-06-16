#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_artic_device.hpp"
#include "fake_rtc.hpp"
#include "fake_config_store.hpp"
#include "fake_timer.hpp"
#include "dte_protocol.hpp"
#include "binascii.hpp"
#include "timeutils.hpp"
#include "scheduler.hpp"
#include "argos_rx_service.hpp"

extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC *rtc;


TEST_GROUP(ArgosRxService)
{
	FakeConfigurationStore *fake_config_store;
	MockArticDevice *mock_artic;
	FakeRTC *fake_rtc;
	FakeTimer *fake_timer;
	unsigned int txco_warmup = 5U;

	void setup() {
		mock_artic = new MockArticDevice;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		configuration_store->init();
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
		fake_timer = new FakeTimer;
		system_timer = fake_timer; // linux_timer;
		system_scheduler = new Scheduler(system_timer);
		fake_timer->start();

		// Setup PP_MIN_ELEVATION for 5.0
		double min_elevation = 5.0;
		fake_config_store->write_param(ParamID::PP_MIN_ELEVATION, min_elevation);
	}

	void teardown() {
		delete system_scheduler;
		delete fake_timer;
		delete fake_rtc;
		delete fake_config_store;
		delete mock_artic;
	}

	void inject_gps_location(double longitude, double latitude) {
		ServiceEvent e;
		GPSLogEntry log;
		log.info.valid = 1;
		log.info.lon = longitude;
		log.info.lat = latitude;
		e.event_source = ServiceIdentifier::GNSS_SENSOR;
		e.event_type = ServiceEventType::SERVICE_LOG_UPDATED;
		e.event_data = log;
		ServiceManager::notify_peer_event(e);
	}

	void notify_underwater_state(bool state) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_LOG_UPDATED,
		e.event_data = state,
		e.event_source = ServiceIdentifier::UW_SENSOR;
		ServiceManager::notify_peer_event(e);
	}
};

TEST(ArgosRxService, BasicDownlinkReceive)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int rx_aop_update_period = 1U;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD, rx_aop_update_period);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
		/* version_code */ 0,
		1,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
		}
	};

	// Write ARGOS_AOP_DATE
	std::time_t last_aop = 0;
	fake_config_store->write_param(ParamID::ARGOS_AOP_DATE, last_aop);
	fake_config_store->write_pass_predict(pass_predict);

	ArgosRxService service(*mock_artic);
	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	service.start(nullptr);

	std::time_t t = 1632009600;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	inject_gps_location(11.8768, -33.8232);

	t += service.get_last_schedule();
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	mock().expectOneCall("start_receive").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A3);
	while (!system_scheduler->run());

	std::string x = "00000C77007A5C900B7C500800C00D4C4224";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	// Throw a time sync packet into the decoder
	x = "00000E17082021262105420895D027";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	// Throw a bad packet into the decoder
	x = "FFFFFFFFFFFF";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	x = "000005F7006601A58900B78C00D48068F6";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000D4400648498489095CCDA39F73865F5B4216060A62B";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE400848498488D122AC553EEDA8B7190084DA9809A";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE400C48498485510E6D7CBECDADB736A075523678D";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE400A4849848890027799D26C6B2FBCF0039356395";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE4009484984856422B159528C6BAFC120042B0A479";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE400B4849848944DE97B1D2AC6AAFBAE0041905A68";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE40054849848C2442523CDCABCA2C02F014173AFE4";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	mock().expectOneCall("stop_receive").onObject(mock_artic).andReturnValue(true);

	x = "00000BE400D4849848904D8D33269EAF627189023C5658A5";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	mock().expectOneCall("stop_receive").onObject(mock_artic).andReturnValue(true);
	mock_artic->notify_rx_stopped(10000);
	mock_artic->notify_off();
	service.stop();

	// Now check that the records have been updated
	pass_predict = configuration_store->read_pass_predict();
	CHECK_EQUAL(8, pass_predict.num_records);

	// Check last AOP date
	std::time_t last_aop_update;
	last_aop_update = configuration_store->read_param<std::time_t>(ParamID::ARGOS_AOP_DATE);
	CHECK_EQUAL(t, last_aop_update);

	// Check RX counter
	unsigned int rx_counter = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_COUNTER);
	CHECK_EQUAL(12, rx_counter);

	// Check RX time
	unsigned int rx_time = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_TIME);
	CHECK_EQUAL(10, rx_time);
}

TEST(ArgosRxService, CancelledOnUnderwaterAndRescheduledOnSurfaced)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int rx_aop_update_period = 0U;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD, rx_aop_update_period);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
		/* version_code */ 0,
		1,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
		}
	};

	// Write ARGOS_AOP_DATE
	std::time_t last_aop = 0;
	fake_config_store->write_param(ParamID::ARGOS_AOP_DATE, last_aop);
	fake_config_store->write_pass_predict(pass_predict);

	ArgosRxService service(*mock_artic);
	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	service.start(nullptr);

	std::time_t t = 1580083200;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	inject_gps_location(11.8768, -33.8232);

	t += 24050;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	mock().expectOneCall("start_receive").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A3);
	system_scheduler->run();

	t += 100;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	mock().expectOneCall("stop_receive").onObject(mock_artic).andReturnValue(true);
	notify_underwater_state(true);

	t += 100;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	notify_underwater_state(false);

	t += 1;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	mock().expectOneCall("start_receive").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A3);
	system_scheduler->run();
}


TEST(ArgosRxService, StillRunsIfSurfacedBeforeSchedule)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int rx_aop_update_period = 0U;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD, rx_aop_update_period);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
		/* version_code */ 0,
		1,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
		}
	};

	// Write ARGOS_AOP_DATE
	std::time_t last_aop = 0;
	fake_config_store->write_param(ParamID::ARGOS_AOP_DATE, last_aop);
	fake_config_store->write_pass_predict(pass_predict);

	ArgosRxService service(*mock_artic);
	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	service.start(nullptr);

	std::time_t t = 1580083200;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	inject_gps_location(11.8768, -33.8232);

	mock().expectOneCall("stop_receive").onObject(mock_artic).andReturnValue(true);
	notify_underwater_state(true);
	notify_underwater_state(false);

	t += 24050000;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	mock().expectOneCall("start_receive").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A3);
	system_scheduler->run();
}


TEST(ArgosRxService, DeferredIfUnderwaterWhenScheduled)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int rx_aop_update_period = 0U;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD, rx_aop_update_period);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
		/* version_code */ 0,
		1,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
		}
	};

	// Write ARGOS_AOP_DATE
	std::time_t last_aop = 0;
	fake_config_store->write_param(ParamID::ARGOS_AOP_DATE, last_aop);
	fake_config_store->write_pass_predict(pass_predict);

	ArgosRxService service(*mock_artic);
	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	service.start(nullptr);

	std::time_t t = 1580083200;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	inject_gps_location(11.8768, -33.8232);

	mock().expectOneCall("stop_receive").onObject(mock_artic).andReturnValue(true);
	notify_underwater_state(true);

#if 1
	t += 24050;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	//mock().expectOneCall("start_receive").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A3);
	system_scheduler->run();
#endif

	//notify_underwater_state(false);
}
