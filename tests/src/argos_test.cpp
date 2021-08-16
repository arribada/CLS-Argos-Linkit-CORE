#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_artic.hpp"
#include "fake_rtc.hpp"
#include "fake_config_store.hpp"
#include "fake_logger.hpp"
#include "linux_timer.hpp"
#include "fake_timer.hpp"
#include "dte_protocol.hpp"
#include "binascii.hpp"


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
		configuration_store->init();
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("FFFE2F61234567343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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

	STRCMP_EQUAL("FFFE2F61234567343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(2, tx_counter);

	// Make sure configuration store has been saved for each transmission
	CHECK_EQUAL(2, fake_config_store->get_saved_count());
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	// Hour 0 - send
	fake_rtc->settime(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();

	STRCMP_EQUAL("FFFE2F61234567343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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

	STRCMP_EQUAL("FFFE2F61234567343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
	unsigned int dloc_arg_nom = 60*60U;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_rtc->settime(7200);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8736;
	gps_entry.info.lat = -33.8235;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 17;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);


	// Should now run with long packet
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("FFFE2FF12345672FE381A949C039FE039FC95293B073F42AD2300E79855A4681CF34812E51F9", Binascii::hexlify(mock_artic->m_last_packet).c_str());

}


TEST(ArgosScheduler, PrepassSchedulingShortPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Not used
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 20;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 45;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	unsigned int dloc_arg_nom = 60*60U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);

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

	// Populate GPS
	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);

	std::time_t t = 1580083200;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	t += 8888;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 0; i < 10; i++) {
		printf("i=%u\n", i);
		mock().expectOneCall("power_on").onObject(argos_sched);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
		mock().expectOneCall("power_off").onObject(argos_sched);

		t += 33;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
		while (!system_scheduler->run());
	}
}

TEST(ArgosScheduler, PrepassSchedulingLongPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Not used
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 20;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 45;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	unsigned int dloc_arg_nom = 60*60U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);

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
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);

	std::time_t t = 1580083200;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 15;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8736;
	gps_entry.info.lat = -33.8235;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 16;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 17;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	t += 8888;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 0; i < 11; i++) {
		printf("i=%u\n", i);
		mock().expectOneCall("power_on").onObject(argos_sched);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
		mock().expectOneCall("power_off").onObject(argos_sched);

		t += 33;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
		while (!system_scheduler->run());
	}
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	fake_timer->set_counter(0);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

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

		fake_log->write(&gps_entry);
		argos_sched->notify_sensor_log_update();
		system_scheduler->run();
	}
}


TEST(ArgosScheduler, DutyCycleWithSaltwaterSwitchEvents)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 10;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 45;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = true;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

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
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();

	t = 10;
	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));

	// SWS: 1
	t = 30;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(true);

	// SWS: 0
	t = 31;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(false);

	// Next schedule at t=45
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	t = 45;
	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 1
	t = 89;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(true);

	// SWS: 0
	t = 92;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(false);

	// Should use earliest TX
	t = 92 + dry_time_before_tx;
	DEBUG_TRACE("************* rtc: %u", t);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// Fall back to TR_NOM
	t = 135;
	DEBUG_TRACE("************* rtc: %u", t);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 1
	t = 140;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(true);

	// Shall not transmit as SWS=1
	t = 181;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 0
	t = 190;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(false);

	// Should use earliest TX
	t += dry_time_before_tx;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

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
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
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

TEST(ArgosScheduler, PrepassWithSaltwaterSwitchEvents)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Every other hour
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 10;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 120;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = true;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

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
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	std::time_t t = 1580083200;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();


	DEBUG_TRACE("*** SWS delays TX schedule ***");

	t += 11878;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();

	// SWS: 1
	t += 10;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(true);

	// SWS: 1
	t += 98;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 0
	t += 50;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(false);

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	t += 10;
	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	DEBUG_TRACE("*** SWS does not delay TX schedule ***");

	// SWS: 1
	t += 30;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(true);

	t += 30;
	DEBUG_TRACE("************* rtc: %u SWS 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 0
	t += 30;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_saltwater_switch_state(false);

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);

	t += 18;
	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();
}

TEST(ArgosScheduler, SchedulingShortPacketWithNonZeroAltitude)
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 400 * 1000;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

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

	STRCMP_EQUAL("FFFE2F61234567B63BC63EA7FC011BE000014FC24C63", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(2, tx_counter);
}

TEST(ArgosScheduler, SchedulingShortPacketWithMaxTruncatedAltitude)
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 15000 * 1000;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("FFFE2F61234567F63BC63EA7FC011BE0001FCFD16604", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
}


TEST(ArgosScheduler, SchedulingShortPacketWithMinTruncatedAltitude)
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
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = -50 * 1000;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

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
}

TEST(ArgosScheduler, SchedulingCheckGpsBurstCount)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_12;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 3;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	unsigned int dloc_arg_nom = 60*60U;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8736;
	gps_entry.info.lat = -33.8235;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 17;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	// Should now run with long packet
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 1; i < 13; i++) {

		if (i < 10) {
			fake_log->write(&gps_entry);
			argos_sched->notify_sensor_log_update();
		}

		if (i < 12) {
			mock().expectOneCall("power_on").onObject(argos_sched);
			mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
			mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
			mock().expectOneCall("send_packet").onObject(argos_sched).ignoreOtherParameters();
			mock().expectOneCall("power_off").onObject(argos_sched);
		}

		// Should now run with long packet
		fake_rtc->settime(i*3600);
		fake_timer->set_counter(i*3600*1000);
		system_scheduler->run();
	}
}


TEST(ArgosScheduler, SchedulingLongPacketLowBatteryFlag)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every other hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 50U;
	bool lb_en = true;
	bool underwater_en = false;
	unsigned int dloc_arg_nom = 60*60U;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::LB_ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_LB, tr_nom);
	fake_config_store->write_param(ParamID::LB_ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_rtc->settime(3600);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8736;
	gps_entry.info.lat = -33.8235;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 17;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	// Should now run with long packet
	fake_config_store->set_battery_level(40);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("FFFE2FF1234567B9E381A949C039FE0383E95293B073F42AD2300E79855A4681CF347642D764", Binascii::hexlify(mock_artic->m_last_packet).c_str());

}


TEST(ArgosScheduler, SchedulingShortPacketLowBatteryFlag)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every other hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 50U;
	bool lb_en = true;
	bool underwater_en = false;
	unsigned int dloc_arg_nom = 60*60U;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::LB_ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_LB, tr_nom);
	fake_config_store->write_param(ParamID::LB_ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_rtc->settime(3600);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	// Should now run with long packet
	fake_config_store->set_battery_level(40);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("FFFE2F6123456768E381A949C039FE03800003FC48B6", Binascii::hexlify(mock_artic->m_last_packet).c_str());

}


TEST(ArgosScheduler, SchedulingShortPacketOutOfZoneFlag)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every other hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 50U;
	bool lb_en = true;
	bool underwater_en = false;
	unsigned int dloc_arg_nom = 60*60U;
	bool sync_burst_en = false;

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
	fake_config_store->write_param(ParamID::LB_ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_LB, tr_nom);
	fake_config_store->write_param(ParamID::LB_ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_rtc->settime(3600);

	// Setup zone file
	BaseZone zone = {
		/* zone_id */ 1,
		/* zone_type */ BaseZoneType::CIRCLE,
		/* enable_monitoring */ true,
		/* enable_entering_leaving_events */ true,
		/* enable_out_of_zone_detection_mode */ true,
		/* enable_activation_date */ false,
		/* year */ 2020,
		/* month */ 1,
		/* day */ 1,
		/* hour */ 0,
		/* minute */ 0,
		/* comms_vector */ BaseCommsVector::UNCHANGED,
		/* delta_arg_loc_argos_seconds */ 3600,
		/* delta_arg_loc_cellular_seconds */ 65,
		/* argos_extra_flags_enable */ true,
		/* argos_depth_pile */ BaseArgosDepthPile::DEPTH_PILE_1,
		/* argos_power */ BaseArgosPower::POWER_200_MW,
		/* argos_time_repetition_seconds */ 240,
		/* argos_mode */ BaseArgosMode::LEGACY,
		/* argos_duty_cycle */ 0xFFFFFFU,
		/* gnss_extra_flags_enable */ true,
		/* hdop_filter_threshold */ 2,
		/* gnss_acquisition_timeout_seconds */ 240,
		/* center_longitude_x */ -123.3925,
		/* center_latitude_y */ -48.8752,
		/* radius_m */ 100
	};
	fake_config_store->write_zone(zone);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	configuration_store->notify_gps_location(gps_entry);

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)zone.argos_power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	// Should now run with short packet
	fake_config_store->set_battery_level(100);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("FFFE2F6123456779E381A949C039FE03A00003C00552", Binascii::hexlify(mock_artic->m_last_packet).c_str());

}

TEST(ArgosScheduler, SchedulingLongPacketOutOfZoneFlag)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every other hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 3600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 50U;
	bool lb_en = true;
	bool underwater_en = false;
	bool sync_burst_en = false;
	unsigned int dloc_arg_nom = 60*60U;

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
	fake_config_store->write_param(ParamID::LB_ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::TR_LB, tr_nom);
	fake_config_store->write_param(ParamID::LB_ARGOS_MODE, mode);
	fake_config_store->write_param(ParamID::LB_ARGOS_DUTY_CYCLE, duty_cycle);
	fake_config_store->write_param(ParamID::LB_TRESHOLD, lb_threshold);
	fake_config_store->write_param(ParamID::UNDERWATER_EN, underwater_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, sync_burst_en);
	fake_rtc->settime(3600);

	// Setup zone file
	BaseZone zone = {
		/* zone_id */ 1,
		/* zone_type */ BaseZoneType::CIRCLE,
		/* enable_monitoring */ true,
		/* enable_entering_leaving_events */ true,
		/* enable_out_of_zone_detection_mode */ true,
		/* enable_activation_date */ false,
		/* year */ 2020,
		/* month */ 1,
		/* day */ 1,
		/* hour */ 0,
		/* minute */ 0,
		/* comms_vector */ BaseCommsVector::UNCHANGED,
		/* delta_arg_loc_argos_seconds */ 3600,
		/* delta_arg_loc_cellular_seconds */ 65,
		/* argos_extra_flags_enable */ true,
		/* argos_depth_pile */ BaseArgosDepthPile::DEPTH_PILE_4,
		/* argos_power */ BaseArgosPower::POWER_200_MW,
		/* argos_time_repetition_seconds */ 240,
		/* argos_mode */ BaseArgosMode::LEGACY,
		/* argos_duty_cycle */ 0xFFFFFFU,
		/* gnss_extra_flags_enable */ true,
		/* hdop_filter_threshold */ 2,
		/* gnss_acquisition_timeout_seconds */ 240,
		/* center_longitude_x */ -123.3925,
		/* center_latitude_y */ -48.8752,
		/* radius_m */ 100
	};
	fake_config_store->write_zone(zone);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8736;
	gps_entry.info.lat = -33.8235;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 17;
	gps_entry.info.min = 1;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	configuration_store->notify_gps_location(gps_entry);

	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)zone.argos_power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 304).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);

	// Should now run with long packet
	fake_config_store->set_battery_level(100);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("FFFE2FF123456725E381A949C039FE03A3C95293B073F42AD2300E79855A4681CF34EA788803", Binascii::hexlify(mock_artic->m_last_packet).c_str());

}

TEST(ArgosScheduler, TimeSyncBurstTransmissionIsSent)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool time_sync_burst_en = true;

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
	fake_config_store->write_param(ParamID::ARGOS_TIME_SYNC_BURST_EN, time_sync_burst_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("total_bits", 176).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("FFFE2F61234567343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	// This log entry should not trigger a time sync burst
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();
}
