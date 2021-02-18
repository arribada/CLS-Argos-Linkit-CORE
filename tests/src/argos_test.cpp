#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_artic.hpp"
#include "fake_rtc.hpp"
#include "fake_config_store.hpp"
#include "fake_logger.hpp"
#include "linux_timer.hpp"
#include "fake_timer.hpp"
#include "dte_protocol.hpp"

extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern ServiceScheduler *comms_scheduler;
extern Scheduler *system_scheduler;
extern Logger *sensor_log;
extern RTC *rtc;

TEST_GROUP(ArgosScheduler)
{
	FakeConfigurationStore *fake_config_store;
	ArgosScheduler *argos_sched;
	MockArtic *mock_artic;
	FakeRTC *fake_rtc;
	LinuxTimer *linux_timer;
	FakeTimer *fake_timer;
	FakeLog *fake_log;

	void setup() {
		mock_artic = new MockArtic;
		argos_sched = mock_artic;
		comms_scheduler = argos_sched;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		fake_log = new FakeLog;
		fake_log->create();
		sensor_log = fake_log;
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
		linux_timer = new LinuxTimer;
		fake_timer = new FakeTimer;
		system_timer = fake_timer; // linux_timer;
		system_scheduler = new Scheduler(system_timer);
		fake_timer->start();
	}

	void teardown() {
		delete system_scheduler;
		delete linux_timer;
		delete fake_timer;
		delete fake_rtc;
		delete fake_log;
		delete fake_config_store;
		delete mock_artic;
	}
};

TEST(ArgosScheduler, LegacyModeSchedulingShortPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 3960;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = -0.2271;
	gps_entry.lat = 51.3279;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;
	gps_entry.headMot = 0;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	fake_rtc->settime(60);
	fake_timer->set_counter(60000);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(2, tx_counter);

	// Reference packet from CLS
#ifdef ARGOS_USE_CRC8
	CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x03\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x95\x4d\x34"s, mock_artic->m_last_packet);
#else
	CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x11\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x85\xd7\x86"s, mock_artic->m_last_packet);
#endif
}

TEST(ArgosScheduler, DutyCycleModeSchedulingShortPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xAAAAAAU; // Every other hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	// Hour 0 - send
	fake_rtc->settime(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 3960;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = -0.2271;
	gps_entry.lat = 51.3279;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();

	// Hour 1 - no transmission, schedule for 7200
	fake_timer->set_counter(3600000);
	fake_rtc->settime(3600);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();

	fake_rtc->settime(7200);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	fake_timer->set_counter(7200000);
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	// Reference packet from CLS
#ifdef ARGOS_USE_CRC8
	CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x03\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x95\x4d\x34"s, mock_artic->m_last_packet);
#else
	CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x11\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x85\xd7\x86"s, mock_artic->m_last_packet);
#endif

}

TEST(ArgosScheduler, SchedulingLongPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xAAAAAAU; // Every other hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	argos_sched->start();

	// Should not run as requires at least 2 GPS entries
	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 7350;
	gps_entry.day = 28;
	gps_entry.hour = 14;
	gps_entry.min = 1;
	gps_entry.valid = 1;
	gps_entry.lon = 11.8768;
	gps_entry.lat = -33.8232;
	gps_entry.height = 0;
	gps_entry.gSpeed = 8056;  // mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);

	gps_entry.batt_voltage = 7350;
	gps_entry.day = 28;
	gps_entry.hour = 15;
	gps_entry.min = 1;
	gps_entry.valid = 1;
	gps_entry.lon = 11.8736;
	gps_entry.lat = -33.8235;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);

	gps_entry.batt_voltage = 7350;
	gps_entry.day = 28;
	gps_entry.hour = 16;
	gps_entry.min = 1;
	gps_entry.valid = 1;
	gps_entry.lon = 11.8576;
	gps_entry.lat = -35.4584;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);

	gps_entry.batt_voltage = 7350;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = 11.858;
	gps_entry.lat = -35.4586;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);

	fake_rtc->settime(7200);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	argos_sched->notify_sensor_log_update();

	// Should now run with long packet
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

#ifdef ARGOS_USE_CRC8
	CHECK_EQUAL("\xff\xfc\x2f\xf1\x23\x45\x67\x67\xe3\x81\xa9\x49\xc0\x39\xfe\x03\xbe\xa9\x52\x93\xb0\x73\xf4\x2a\xd2\x30\x0e\x79\x85\x5a\x46\x81\xcf\x34\x91\x46\xe2\x79"s, mock_artic->m_last_packet);
#else
	CHECK_EQUAL("\xff\xfc\x2f\xF1\x23\x45\x67\x8B\xE3\x81\xA9\x49\xC0\x39\xFE\x03\xBE\xA9\x52\x93\xB0\x73\xF4\x2A\xD2\x30\x0E\x79\x85\x5A\x46\x81\xCF\x34\x89\xA3\xAA\xC9"s, mock_artic->m_last_packet);
#endif
}

TEST(ArgosScheduler, PrepassSchedulingShortPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Every other hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 0;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
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

	fake_config_store->write_pass_predict(pass_predict);

	// Start the scheduler
	argos_sched->start();

	// Write initial GPS entry
	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 3960;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = -0.2271;
	gps_entry.lat = 51.3279;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;
	gps_entry.headMot = 0;

	fake_rtc->settime(1580083200);
	fake_timer->set_counter(0);

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	fake_timer->set_counter(11910000);
	while (!system_scheduler->run());

	// Reference packet from CLS
#ifdef ARGOS_USE_CRC8
	CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x03\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x95\x4d\x34"s, mock_artic->m_last_packet);
#else
	CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x11\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x85\xd7\x86"s, mock_artic->m_last_packet);
#endif
}

TEST(ArgosScheduler, PrepassSchedulingLongPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Not used
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 0;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
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

	fake_config_store->write_pass_predict(pass_predict);

	// Start the scheduler
	argos_sched->start();

	// Populate 4 GPS entries
	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 7350;
	gps_entry.day = 28;
	gps_entry.hour = 14;
	gps_entry.min = 1;
	gps_entry.valid = 1;
	gps_entry.lon = 11.8768;
	gps_entry.lat = -33.8232;
	gps_entry.height = 0;
	gps_entry.gSpeed = 8056;  // mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);

	fake_rtc->settime(1580083200);
	fake_timer->set_counter(0);
	argos_sched->notify_sensor_log_update();

	gps_entry.batt_voltage = 7350;
	gps_entry.day = 28;
	gps_entry.hour = 15;
	gps_entry.min = 1;
	gps_entry.valid = 1;
	gps_entry.lon = 11.8736;
	gps_entry.lat = -33.8235;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.batt_voltage = 7350;
	gps_entry.day = 28;
	gps_entry.hour = 16;
	gps_entry.min = 1;
	gps_entry.valid = 1;
	gps_entry.lon = 11.8576;
	gps_entry.lat = -35.4584;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.batt_voltage = 7350;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = 11.858;
	gps_entry.lat = -35.4586;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	fake_timer->set_counter(8910000);
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

#ifdef ARGOS_USE_CRC8
	CHECK_EQUAL("\xff\xfc\x2f\xf1\x23\x45\x67\x67\xe3\x81\xa9\x49\xc0\x39\xfe\x03\xbe\xa9\x52\x93\xb0\x73\xf4\x2a\xd2\x30\x0e\x79\x85\x5a\x46\x81\xcf\x34\x91\x46\xe2\x79"s, mock_artic->m_last_packet);
#else
	CHECK_EQUAL("\xff\xfc\x2f\xF1\x23\x45\x67\x8B\xE3\x81\xA9\x49\xC0\x39\xFE\x03\xBE\xA9\x52\x93\xB0\x73\xF4\x2A\xD2\x30\x0E\x79\x85\x5A\x46\x81\xCF\x34\x89\xA3\xAA\xC9"s, mock_artic->m_last_packet);
#endif
}


TEST(ArgosScheduler, DutyCycleModeManyShortPackets)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xAAAAAAU; // Every other hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 45;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	fake_timer->set_counter(0);
	argos_sched->start();

	for (unsigned int i = 0; i < 24*3600; i += tr_nom)
	{
		DEBUG_TRACE("************* rtc: %u", i);
		fake_rtc->settime(i);
		fake_timer->set_counter((i*1000));
		if ((i / 3600) % 2 == 0)
		{
			mock().expectOneCall("power_on").onObject(argos_sched);
			mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
			mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
			mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
			mock().expectOneCall("power_off").onObject(argos_sched);
		}

		GPSLogEntry gps_entry;
		gps_entry.batt_voltage = 3960;
		gps_entry.day = 7;
		gps_entry.hour = 15;
		gps_entry.min = 6;
		gps_entry.valid = 1;
		gps_entry.lon = -0.2271;
		gps_entry.lat = 51.3279;
		gps_entry.height = 0;
		gps_entry.gSpeed = 0;
		gps_entry.headMot = 0;
		fake_log->write(&gps_entry);
		argos_sched->notify_sensor_log_update();
		if ((i / 3600) % 2 == 0)
			while (!system_scheduler->run());

		// Reference packet from CLS
	#ifdef ARGOS_USE_CRC8
		CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x03\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x95\x4d\x34"s, mock_artic->m_last_packet);
	#else
		CHECK_EQUAL("\xff\xfc\x2f\x61\x23\x45\x67\x11\x3b\xc6\x3e\xa7\xfc\x01\x1b\xe0\x00\x00\x10\x85\xd7\x86"s, mock_artic->m_last_packet);
	#endif
	}
}


TEST(ArgosScheduler, DutyCycleWithSaltwaterSwitchSuppression)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 45;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = true;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	argos_sched->start();

	unsigned int t = 0;

	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	fake_timer->set_counter((t*1000));

	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 3960;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = -0.2271;
	gps_entry.lat = 51.3279;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	t = 10;
	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	// SWS: 1 => should cancel existing schedule
	t = 40;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	argos_sched->notify_saltwater_switch_state(true);

	// SWS: 1 => ensure scheduler is not scheduled
	CHECK_FALSE(system_scheduler->is_any_task_scheduled());

	// SWS: 0 => should re-schedule to earliest T+10
	t = 50;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(false);
	CHECK_TRUE(system_scheduler->is_any_task_scheduled());

	// Next schedule at t=90
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	t = 90;
	fake_timer->set_counter((t*1000));
	while (!system_scheduler->run());

}

TEST(ArgosScheduler, RescheduleAfterTransmissionWithoutNewSensorDataNBurstTimes)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 3;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 45;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = true;

	fake_config_store->write_param(ParamID::ARGOS_DEPTH_PILE, depth_pile);
	fake_config_store->write_param(ParamID::DRY_TIME_BEFORE_TX, dry_time_before_tx);
	fake_config_store->write_param(ParamID::DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::NTRY_PER_MESSAGE, ntry_per_message);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::TR_NOM, tr_nom);
	fake_config_store->write_param(ParamID::TX_COUNTER, tx_counter);
	fake_config_store->write_param(ParamID::LB_EN, lb_en);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);

	argos_sched->start();

	unsigned int t = 0;

	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	GPSLogEntry gps_entry;
	gps_entry.batt_voltage = 3960;
	gps_entry.day = 7;
	gps_entry.hour = 15;
	gps_entry.min = 6;
	gps_entry.valid = 1;
	gps_entry.lon = -0.2271;
	gps_entry.lat = 51.3279;
	gps_entry.height = 0;
	gps_entry.gSpeed = 0;
	gps_entry.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());
	CHECK_TRUE(system_scheduler->is_any_task_scheduled());

	t += tr_nom;

	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	while (!system_scheduler->run());
	CHECK_TRUE(system_scheduler->is_any_task_scheduled());

	t += tr_nom;

	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	while (!system_scheduler->run());
	CHECK_TRUE(system_scheduler->is_any_task_scheduled());

	t += tr_nom;

	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	// !!! No packet should be sent !!!
	while (!system_scheduler->run());
	CHECK_TRUE(system_scheduler->is_any_task_scheduled());

}
