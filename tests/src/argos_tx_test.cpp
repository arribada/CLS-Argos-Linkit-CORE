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
#include "argos_tx_service.hpp"

extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC *rtc;


TEST_GROUP(ArgosTxService)
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

	GPSLogEntry make_gps_location(double longitude=0, double latitude=0, std::time_t t=0, bool is_3d_fix = false, int32_t hMSL=0, int32_t gSpeed=0, uint16_t batt=4200) {
		GPSLogEntry log;
		log.info.valid = 1;
		log.info.lon = longitude;
		log.info.lat = latitude;
		log.info.schedTime = t;
		log.info.fixType = is_3d_fix ? 3 : 2;
		log.info.hMSL = hMSL;
		log.info.batt_voltage = batt;
		log.info.gSpeed = gSpeed;

		uint16_t year;
		uint8_t month, day, hour, min, sec;
		convert_datetime_to_epoch(t, year, month, day, hour, min, sec);
		log.header.year = log.info.year = year;
		log.header.month = log.info.month = month;
		log.header.day = log.info.day = day;
		log.header.hours = log.info.hour = hour;
		log.header.minutes = log.info.min = min;
		log.header.seconds = log.info.sec = sec;
		return log;
	}

	void inject_gps_location(double longitude=0, double latitude=0, std::time_t t=0) {
		ServiceEvent e;
		GPSLogEntry log = make_gps_location(longitude, latitude, t);

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

TEST(ArgosTxService, DepthPileFillsAndEmpties)
{
	ArgosDepthPile<GPSLogEntry> dp;
	std::vector<GPSLogEntry*> v;
	GPSLogEntry e;

	// Load up full depth pile with burst count of 1
	for (unsigned int i = 0; i < 24; i++) {
		e.info.day = i;
		dp.store(e, 1);
	}

	// Should have 24 eligible entries
	CHECK_EQUAL(24, dp.eligible());

	// Retrieving the latest entry should not decrement burst counter
	// for time sync burst case
	v = dp.retrieve_latest();
	CHECK_EQUAL(1, v.size());
	CHECK_EQUAL(24, dp.eligible());
	CHECK_EQUAL(23, v.at(0)->info.day); // Should be most recent

	// Retrieve entire depth pile (in blocks of 4) ensuring
	// eligible count is decremented
	for (unsigned int i = 0; i < 6; i++) {
		//CHECK_EQUAL(24-i, dp.eligible());
		v = dp.retrieve(24);
		CHECK_EQUAL(4, v.size());
		CHECK_EQUAL(20-(i*4), dp.eligible());
		for (unsigned j = 0; j < 4; j++) {
			CHECK_EQUAL(24-((i+1)*4)+j, v.at(j)->info.day);
		}
	}

	// Check depth pile returns empty vector
	v = dp.retrieve_latest();
	CHECK_EQUAL(0, v.size());
	v = dp.retrieve(24);
	CHECK_EQUAL(0, v.size());
}

TEST(ArgosTxService, DutyCycleCalculation)
{
	unsigned int mask;
	mask = 0xFFFFFF;
	for (unsigned int i = 0; i < 24; i++)
		CHECK_EQUAL((bool)((mask >> (23-i))&1), ArgosTxScheduler::is_in_duty_cycle(i*3600*1000, mask));
	mask = 0xEEEEEE;
	for (unsigned int i = 0; i < 24; i++)
		CHECK_EQUAL((bool)((mask >> (23-i))&1), ArgosTxScheduler::is_in_duty_cycle(i*3600*1000, mask));
	mask = 0;
	for (unsigned int i = 0; i < 24; i++)
		CHECK_EQUAL((bool)((mask >> (23-i))&1), ArgosTxScheduler::is_in_duty_cycle(i*3600*1000, mask));
	mask = 0x123456;
	for (unsigned int i = 0; i < 24; i++)
		CHECK_EQUAL((bool)((mask >> (23-i))&1), ArgosTxScheduler::is_in_duty_cycle(i*3600*1000, mask));
}

TEST(ArgosTxService, SchedulerLegacyNoJitter)
{
	ArgosTxScheduler sched;
	ArgosConfig config;

	config.argos_tx_jitter_en = false;
	config.tr_nom = 10;
	unsigned int result;

	result = sched.schedule_legacy(config, 0);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 10);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 20);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 30);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 35);
	CHECK_EQUAL(5000U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 59);
	CHECK_EQUAL(1000U, result);
	sched.notify_tx_complete();
}

TEST(ArgosTxService, SchedulerLegacyNoJitterWithEarliestTxSet)
{
	ArgosTxScheduler sched;
	ArgosConfig config;

	config.argos_tx_jitter_en = false;
	config.tr_nom = 10;
	unsigned int result;

	result = sched.schedule_legacy(config, 0);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 10);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 20);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 30);
	CHECK_EQUAL(0U, result);
	sched.notify_tx_complete();
	sched.set_earliest_schedule(41);
	result = sched.schedule_legacy(config, 35);
	CHECK_EQUAL(6000U, result);
	sched.notify_tx_complete();
}

TEST(ArgosTxService, SchedulerLegacyWithJitter)
{
	ArgosTxScheduler sched;
	ArgosConfig config;

	config.argos_tx_jitter_en = true;
	config.tr_nom = 10;
	unsigned int result;

	result = sched.schedule_legacy(config, 0);
	CHECK_TRUE(result <= 5000);
	sched.notify_tx_complete();
	result = sched.schedule_legacy(config, 0);
	CHECK_TRUE(result <= 15000 && result >= 10000);
	sched.notify_tx_complete();
}

TEST(ArgosTxService, SchedulerDutyCycleNoJitter)
{
	ArgosTxScheduler sched;
	ArgosConfig config;

	config.argos_tx_jitter_en = false;
	config.tr_nom = 10;
	config.duty_cycle = 0x1;

	unsigned int result = sched.schedule_duty_cycle(config, 0);
	CHECK_EQUAL(23*3600*1000, result);
	sched.notify_tx_complete();
	result = sched.schedule_duty_cycle(config, 23*3600);
	CHECK_EQUAL(10000U, result);
	sched.notify_tx_complete();
}

TEST(ArgosTxService, SchedulerPrepassNoLocationSet)
{
	ArgosTxScheduler sched;
	ArgosConfig config;
	ArticMode mode;

	config.argos_tx_jitter_en = false;
	config.tr_nom = 10;

	BasePassPredict pass_predict = {
		/* version_code */ 0,
		7,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
			{ 0x9, 3, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 33, 39 }, 7195.632f, 98.7141f, 344.177f, -25.340f, 101.3600f, 0.00f },
			{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 23, 29, 29 }, 7194.917f, 98.7183f, 330.404f, -25.336f, 101.3449f, 0.00f },
			{ 0x5, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 23, 50, 6 }, 7180.549f, 98.7298f, 289.399f, -25.260f, 101.0419f, -1.78f },
			{ 0x8, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 22, 12, 6 }, 7226.170f, 99.0661f, 343.180f, -25.499f, 102.0039f, -1.80f },
			{ 0xC, 6, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 52 }, 7226.509f, 99.1913f, 291.936f, -25.500f, 102.0108f, -1.98f },
			{ 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 53 }, 7160.246f, 98.5358f, 118.029f, -25.154f, 100.6148f, 0.00f }
		}
	};

	CHECK_EQUAL(ArgosTxScheduler::INVALID_SCHEDULE, sched.schedule_prepass(config, pass_predict, mode, 0));
}

TEST(ArgosTxService, SchedulerPrepassNominal)
{
	ArgosTxScheduler sched;
	ArgosConfig config;
	ArticMode mode;

	config.argos_tx_jitter_en = false;
	config.tr_nom = 10;
	config.prepass_comp_step = 10;
	config.prepass_linear_margin = 300;
	config.prepass_max_passes = 1000;
	config.prepass_min_duration = 30;
	config.prepass_min_elevation = 15;
	config.prepass_max_elevation = 90;

	BasePassPredict pass_predict = {
		/* version_code */ 0,
		7,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
			{ 0x9, 3, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 33, 39 }, 7195.632f, 98.7141f, 344.177f, -25.340f, 101.3600f, 0.00f },
			{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 23, 29, 29 }, 7194.917f, 98.7183f, 330.404f, -25.336f, 101.3449f, 0.00f },
			{ 0x5, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 23, 50, 6 }, 7180.549f, 98.7298f, 289.399f, -25.260f, 101.0419f, -1.78f },
			{ 0x8, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 22, 12, 6 }, 7226.170f, 99.0661f, 343.180f, -25.499f, 102.0039f, -1.80f },
			{ 0xC, 6, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 52 }, 7226.509f, 99.1913f, 291.936f, -25.500f, 102.0108f, -1.98f },
			{ 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 53 }, 7160.246f, 98.5358f, 118.029f, -25.154f, 100.6148f, 0.00f }
		}
	};

	sched.set_last_location(0, 0);
	unsigned int result = sched.schedule_prepass(config, pass_predict, mode, 1652100787U);
	CHECK_EQUAL(15040000U, result);
	CHECK_EQUAL(ArticMode::A3, mode);
	sched.notify_tx_complete();
	result = sched.schedule_prepass(config, pass_predict, mode, 1652115827U);
	CHECK_EQUAL(10000U, result);
	CHECK_EQUAL(ArticMode::A3, mode);
	sched.notify_tx_complete();
	result = sched.schedule_prepass(config, pass_predict, mode, 1652115837U);
	CHECK_EQUAL(10000U, result);
	CHECK_EQUAL(ArticMode::A3, mode);
	sched.notify_tx_complete();
	result = sched.schedule_prepass(config, pass_predict, mode, 1652119000U);
	CHECK_EQUAL(1670000U, result);
	CHECK_EQUAL(ArticMode::A3, mode);
	sched.notify_tx_complete();
}

TEST(ArgosTxService, SchedulerPrepassEmpty)
{
	ArgosTxScheduler sched;
	ArgosConfig config;
	ArticMode mode;

	config.argos_tx_jitter_en = false;
	config.tr_nom = 10;
	config.prepass_comp_step = 10;
	config.prepass_linear_margin = 300;
	config.prepass_max_passes = 1000;
	config.prepass_min_duration = 30;
	config.prepass_min_elevation = 15;
	config.prepass_max_elevation = 90;

	BasePassPredict pass_predict = {
	};

	sched.set_last_location(0, 0);
	unsigned int result = sched.schedule_prepass(config, pass_predict, mode, 1652100787U);
	CHECK_EQUAL(ArgosTxScheduler::INVALID_SCHEDULE, result);
}

TEST(ArgosTxService, SchedulerPrepassWithJitter)
{
	ArgosTxScheduler sched;
	ArgosConfig config;
	ArticMode mode;

	config.argos_tx_jitter_en = true;
	config.tr_nom = 10;
	config.prepass_comp_step = 10;
	config.prepass_linear_margin = 300;
	config.prepass_max_passes = 1000;
	config.prepass_min_duration = 30;
	config.prepass_min_elevation = 15;
	config.prepass_max_elevation = 90;

	BasePassPredict pass_predict = {
		/* version_code */ 0,
		7,
		{
		    { 0xA, 5, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 59, 44 }, 7195.550f, 98.5444f, 327.835f, -25.341f, 101.3587f, 0.00f },
			{ 0x9, 3, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 33, 39 }, 7195.632f, 98.7141f, 344.177f, -25.340f, 101.3600f, 0.00f },
			{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 23, 29, 29 }, 7194.917f, 98.7183f, 330.404f, -25.336f, 101.3449f, 0.00f },
			{ 0x5, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 23, 50, 6 }, 7180.549f, 98.7298f, 289.399f, -25.260f, 101.0419f, -1.78f },
			{ 0x8, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 22, 12, 6 }, 7226.170f, 99.0661f, 343.180f, -25.499f, 102.0039f, -1.80f },
			{ 0xC, 6, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 52 }, 7226.509f, 99.1913f, 291.936f, -25.500f, 102.0108f, -1.98f },
			{ 0xD, 4, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 53 }, 7160.246f, 98.5358f, 118.029f, -25.154f, 100.6148f, 0.00f }
		}
	};

	sched.set_last_location(0, 0);
	unsigned int result = sched.schedule_prepass(config, pass_predict, mode, 1652100787U);
	CHECK_EQUAL(15043148U, result);
	CHECK_EQUAL(ArticMode::A3, mode);
	sched.notify_tx_complete();
	result = sched.schedule_prepass(config, pass_predict, mode, 1652100787U);
	CHECK_EQUAL(15049502U, result);
	CHECK_EQUAL(ArticMode::A3, mode);
	sched.notify_tx_complete();
}

TEST(ArgosTxService, BuildLongCertificationPacket)
{
	unsigned int size_bits;
	std::string x = ArgosPacketBuilder::build_certification_packet("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", size_bits);
	CHECK_EQUAL(248, size_bits);
	CHECK_EQUAL("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"s, x);
}

TEST(ArgosTxService, BuildShortCertificationPacket)
{
	unsigned int size_bits;
	std::string x = ArgosPacketBuilder::build_certification_packet("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", size_bits);
	CHECK_EQUAL(120, size_bits);
	CHECK_EQUAL("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"s, x);
}

TEST(ArgosTxService, BuildShortGNSSPacket)
{
	unsigned int size_bits;
	GPSLogEntry e = make_gps_location(12.3, 44.4, 1652105502);
	std::vector<GPSLogEntry*> v({&e});
	std::string x = ArgosPacketBuilder::build_gnss_packet(v, false, false, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("F94B8B3633003C0F00001FF2C51564"s, Binascii::hexlify(x));

	x = ArgosPacketBuilder::build_gnss_packet(v, true, false, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("EF4B8B3633003C0F00201FF2C5F2F5"s, Binascii::hexlify(x));

	x = ArgosPacketBuilder::build_gnss_packet(v, false, true, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("FE4B8B3633003C0F00001FF2F9BF11"s, Binascii::hexlify(x));

	x = ArgosPacketBuilder::build_gnss_packet(v, true, true, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("E84B8B3633003C0F00201FF2F95880"s, Binascii::hexlify(x));
}


TEST(ArgosTxService, BuildLongGNSSPacket)
{
	unsigned int size_bits;
	GPSLogEntry e = make_gps_location(12.3, 44.4, 1652105502);
	std::vector<GPSLogEntry*> v({&e, &e});
	std::string x = ArgosPacketBuilder::build_gnss_packet(v, false, false, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("C74B8B3633003C0F0012C26C6600781E3FFFFFFFFFFFFFFFFFFFFF6DD38EA7"s, Binascii::hexlify(x));
	v = {&e, &e, &e};
	x = ArgosPacketBuilder::build_gnss_packet(v, false, false, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("7B4B8B3633003C0F0012C26C6600781E0D8CC00F03C7FFFFFFFFFFB8928747"s, Binascii::hexlify(x));
	v = {&e, &e, &e, &e};
	x = ArgosPacketBuilder::build_gnss_packet(v, false, false, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("684B8B3633003C0F0012C26C6600781E0D8CC00F03C1B19801E0788A14F4C3"s, Binascii::hexlify(x));
	x = ArgosPacketBuilder::build_gnss_packet(v, true, false, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("334B8B3633003C0F0032C26C6600781E0D8CC00F03C1B19801E07868E736A3"s, Binascii::hexlify(x));
	x = ArgosPacketBuilder::build_gnss_packet(v, false, true, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("AF4B8B3633003C0F0012E26C6600781E0D8CC00F03C1B19801E078F4DD69C4"s, Binascii::hexlify(x));
	x = ArgosPacketBuilder::build_gnss_packet(v, true, true, BaseDeltaTimeLoc::DELTA_T_10MIN, size_bits);
	CHECK_EQUAL("F44B8B3633003C0F0032E26C6600781E0D8CC00F03C1B19801E078162EABA4"s, Binascii::hexlify(x));
	x = ArgosPacketBuilder::build_gnss_packet(v, true, true, BaseDeltaTimeLoc::DELTA_T_30MIN, size_bits);
	CHECK_EQUAL("CC4B8B3633003C0F0032E66C6600781E0D8CC00F03C1B19801E07814400ECE"s, Binascii::hexlify(x));
}

TEST(ArgosTxService, TimeSyncBurst)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	BaseArgosPower power = BaseArgosPower::POWER_1000_MW;
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int tr_nom = 10;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);

	ArgosTxService serv(*mock_artic);

	std::time_t t = 1652105502000;
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);

	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	mock().expectOneCall("set_tx_power").onObject(mock_artic).withUnsignedIntParameter("power", (unsigned int)power);
	serv.start();

	// First TX is time sync burst
	mock().expectOneCall("send").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A2).
			withUnsignedIntParameter("size_bits", 120);

	inject_gps_location(11.8768, -33.8232, t);
	system_scheduler->run();

	mock_artic->notify(ArticEventTxComplete({}));

	mock().expectOneCall("stop_send").onObject(mock_artic);
	serv.stop();

	// No time sync should be scheduled now
	bool time_sync_en = false;
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, time_sync_en);
	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	mock().expectOneCall("set_tx_power").onObject(mock_artic).withUnsignedIntParameter("power", (unsigned int)power);
	serv.start();
	inject_gps_location(11.8768, -33.8232, t);
	system_scheduler->run();
}

TEST(ArgosTxService, LegacyTxService)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	BaseArgosPower power = BaseArgosPower::POWER_1000_MW;
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int tr_nom = 10;
	bool time_sync_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, time_sync_en);

	ArgosTxService serv(*mock_artic);

	std::time_t t = 1652105502000;
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);

	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	mock().expectOneCall("set_tx_power").onObject(mock_artic).withUnsignedIntParameter("power", (unsigned int)power);
	serv.start();

	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	system_scheduler->run();

	// Subsequent TX will be long packets
	for (unsigned int i = 0; i < 10; i++) {
		mock().expectOneCall("send").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A2).
				withUnsignedIntParameter("size_bits", 248);

		t += serv.get_last_schedule();
		fake_rtc->settime(t/1000);
		fake_timer->set_counter(t);
		system_scheduler->run();

		mock_artic->notify(ArticEventTxComplete({}));
	}
}

TEST(ArgosTxService, TxServiceCancelledBeforeTx)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	BaseArgosPower power = BaseArgosPower::POWER_1000_MW;
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int tr_nom = 10;
	bool time_sync_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, time_sync_en);

	ArgosTxService serv(*mock_artic);

	std::time_t t = 1652105502000;
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);

	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	mock().expectOneCall("set_tx_power").onObject(mock_artic).withUnsignedIntParameter("power", (unsigned int)power);
	serv.start();

	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	system_scheduler->run();

	// Inject UW event
	mock().expectOneCall("stop_send").onObject(mock_artic);
	notify_underwater_state(true);

	// Should result in no transmission
	t += serv.get_last_schedule();
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);
	system_scheduler->run();

	// Inject surfaced event
	notify_underwater_state(false);

	// Next schedule should be equal to dry time before TX since the last TX was deferred
	unsigned int dry_time_before_tx = fake_config_store->read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX);
	CHECK_EQUAL(dry_time_before_tx*1000, serv.get_last_schedule());

	// Should now transmit
	mock().expectOneCall("send").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A2).
			withUnsignedIntParameter("size_bits", 248);
	t += serv.get_last_schedule();
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);
	system_scheduler->run();
}

TEST(ArgosTxService, TxServiceCancelledDuringTx)
{
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	BaseArgosPower power = BaseArgosPower::POWER_1000_MW;
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	unsigned int tr_nom = 10;
	bool time_sync_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, time_sync_en);

	ArgosTxService serv(*mock_artic);

	std::time_t t = 1652105502000;
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);

	mock().expectOneCall("set_frequency").onObject(mock_artic).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(mock_artic).withUnsignedIntParameter("time", 5);
	mock().expectOneCall("set_tx_power").onObject(mock_artic).withUnsignedIntParameter("power", (unsigned int)power);
	serv.start();

	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	inject_gps_location(11.8768, -33.8232, t);
	system_scheduler->run();

	mock().expectOneCall("send").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A2).
			withUnsignedIntParameter("size_bits", 248);

	// TX should start
	t += serv.get_last_schedule();
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);
	system_scheduler->run();

	// Inject UW event
	mock().expectOneCall("stop_send").onObject(mock_artic);
	notify_underwater_state(true);

	// Inject surfaced event
	notify_underwater_state(false);

	// Next schedule should be equal to dry time before TX since the last TX was deferred
	unsigned int dry_time_before_tx = fake_config_store->read_param<unsigned int>(ParamID::DRY_TIME_BEFORE_TX);
	CHECK_EQUAL(dry_time_before_tx*1000, serv.get_last_schedule());

	// Should now transmit
	mock().expectOneCall("send").onObject(mock_artic).withUnsignedIntParameter("mode", (unsigned int)ArticMode::A2).
			withUnsignedIntParameter("size_bits", 248);
	t += serv.get_last_schedule();
	fake_rtc->settime(t/1000);
	fake_timer->set_counter(t);
	system_scheduler->run();
	mock_artic->notify(ArticEventTxComplete({}));
}
