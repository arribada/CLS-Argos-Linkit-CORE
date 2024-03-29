#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "mock_battery_mon.hpp"
#include "mock_artic.hpp"
#include "fake_rtc.hpp"
#include "fake_config_store.hpp"
#include "fake_logger.hpp"
#include "linux_timer.hpp"
#include "fake_timer.hpp"
#include "dte_protocol.hpp"
#include "binascii.hpp"
#include "timeutils.hpp"


extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern ServiceScheduler *comms_scheduler;
extern Scheduler *system_scheduler;
extern RTC *rtc;
extern BatteryMonitor *battery_monitor;


TEST_GROUP(ArgosScheduler)
{
	FakeConfigurationStore *fake_config_store;
	ArgosScheduler *argos_sched;
	MockArtic *mock_artic;
	MockBatteryMonitor *mock_battery;
	FakeRTC *fake_rtc;
	LinuxTimer *linux_timer;
	FakeTimer *fake_timer;
	FakeLog *fake_log;
	unsigned int txco_warmup = 5U;

	void setup() {
		mock_battery = new MockBatteryMonitor;
		battery_monitor = mock_battery;
		mock_artic = new MockArtic;
		argos_sched = mock_artic;
		comms_scheduler = argos_sched;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		configuration_store->init();
		fake_log = new FakeLog("sensor.log");
		fake_log->create();
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
		linux_timer = new LinuxTimer;
		fake_timer = new FakeTimer;
		system_timer = fake_timer; // linux_timer;
		system_scheduler = new Scheduler(system_timer);
		fake_timer->start();

		// Setup PP_MIN_ELEVATION for 5.0
		double min_elevation = 5.0;
		fake_config_store->write_param(ParamID::PP_MIN_ELEVATION, min_elevation);

		// Setup ARGOS_TCXO_WARMUP_TIME
		fake_config_store->write_param(ParamID::ARGOS_TCXO_WARMUP_TIME, txco_warmup);
	}

	void teardown() {
		delete system_scheduler;
		delete linux_timer;
		delete fake_timer;
		delete fake_rtc;
		delete fake_log;
		delete fake_config_store;
		delete mock_artic;
		delete mock_battery;
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
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
	gps_entry.info.onTime = 0;
	gps_entry.info.schedTime = 0;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	fake_rtc->settime(60);
	fake_timer->set_counter(60000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	// Hour 0 - send
	fake_rtc->settime(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
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
	gps_entry.info.onTime = 0;
	gps_entry.info.schedTime = 0;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();

	STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	// Hour 1 - no transmission, schedule for 7200
	fake_timer->set_counter(3600000);
	fake_rtc->settime(3600);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();

	fake_rtc->settime(7200);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	fake_timer->set_counter(7200000);
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_rtc->settime(7200);
	fake_timer->set_counter(7200 * 1000);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 15;
	gps_entry.header.minutes = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 17;
	gps_entry.header.minutes = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with long packet
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("32E441AB48D039E6801FC956918073CC2A52760E7E854A4E01CFF0EDD5BE61", Binascii::hexlify(mock_artic->m_last_packet).c_str());
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);

	// Sample configuration provided with prepass library V3.4
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += 8900;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 0; i < 7; i++) {
		printf("i=%u\n", i);
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

		t += 45;
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);

	// Sample configuration provided with prepass library V3.4
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.header.year, gps_entry.header.month, gps_entry.header.day, gps_entry.header.hours, gps_entry.header.minutes, gps_entry.header.seconds);
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

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += 8900;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 0; i < 8; i++) {
		printf("i=%u\n", i);
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

		t += 45;
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_timer->set_counter(0);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
			mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
			mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
			mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
			mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
			mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
			mock().expectOneCall("power_off").onObject(argos_sched);
			mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
			mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	argos_sched->start();

	unsigned int t = 0;

	DEBUG_TRACE("************* rtc: %u", t);
	fake_rtc->settime(t);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	fake_timer->set_counter((t*1000));

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	argos_sched->notify_underwater_state(true);

	// SWS: 0
	t = 31;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_underwater_state(false);

	// Next schedule at t=45
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
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
	argos_sched->notify_underwater_state(true);

	// SWS: 0
	t = 92;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_underwater_state(false);

	// Should use earliest TX
	t = 92 + dry_time_before_tx;
	DEBUG_TRACE("************* rtc: %u", t);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// Fall back to TR_NOM
	t = 135;
	DEBUG_TRACE("************* rtc: %u", t);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 1
	t = 140;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_underwater_state(true);

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
	argos_sched->notify_underwater_state(false);

	// Should use earliest TX
	t += dry_time_before_tx;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	// Sample configuration provided with prepass library V3.4
	BasePassPredict pass_predict = {
		/* version_code */ 0,
		7,
		{
			{ 0x9, 3, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 33, 39 }, 7195.632f, 98.7141f, 344.177f, -25.340f, 101.3600f, 0.00f },
			{ 0xB, 7, SAT_DNLK_ON_WITH_A3, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 23, 29, 29 }, 7194.917f, 98.7183f, 330.404f, -25.336f, 101.3449f, 0.00f },
			{ 0x5, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 23, 50, 6 }, 7180.549f, 98.7298f, 289.399f, -25.260f, 101.0419f, -1.78f },
			{ 0x8, 0, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A2, { 2020, 1, 26, 22, 12, 6 }, 7226.170f, 99.0661f, 343.180f, -25.499f, 102.0039f, -1.80f },
			{ 0xC, 6, SAT_DNLK_OFF, SAT_UPLK_ON_WITH_A3, { 2020, 1, 26, 22, 3, 52 }, 7226.509f, 99.1913f, 291.936f, -25.500f, 102.0108f, -1.98f },
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();


	DEBUG_TRACE("*** SWS delays TX schedule ***");

	t += 19000;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();

	// SWS: 1
	t += 10;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_underwater_state(true);

	// SWS: 1
	t += 400;
	DEBUG_TRACE("************* rtc: %u SWS: 1", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	system_scheduler->run();

	// SWS: 0
	t += 50;
	DEBUG_TRACE("************* rtc: %u SWS: 0", t);
	fake_rtc->settime(t);
	fake_timer->set_counter((t*1000));
	argos_sched->notify_underwater_state(false);

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
	argos_sched->notify_underwater_state(true);

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
	argos_sched->notify_underwater_state(false);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += 30;
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	STRCMP_EQUAL("B63BC63EA7FC011BE000014FC24C63", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	STRCMP_EQUAL("F63BC63EA7FC011BE0001FCFD16604", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	fake_rtc->settime(60);
	fake_timer->set_counter(60000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	while (!system_scheduler->run());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(2, tx_counter);
}

TEST(ArgosScheduler, SchedulingCheckGpsBurstCountInfiniteInDutyCycleMode)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every hour
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with long packet
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 1; i < 50; i++) {

		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).ignoreOtherParameters();
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

		// Should now run with long packet
		fake_rtc->settime(i*3600);
		fake_timer->set_counter(i*3600*1000);
		system_scheduler->run();
	}
}

TEST(ArgosScheduler, SchedulingCheckGpsBurstCountInfiniteInLegacyMode)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xFFFFFFU; // Every hour
	double frequency = 900.33;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
#if 0
	gps_entry.info.batt_voltage = 7350;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
#endif
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with long packet
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 1; i < 50; i++) {

		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).ignoreOtherParameters();
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 15;
	gps_entry.header.minutes = 1;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 17;
	gps_entry.header.minutes = 1;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 17;
	gps_entry.info.min = 1;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.858;
	gps_entry.info.lat = -35.4586;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with long packet
	fake_config_store->set_battery_level(40);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("A4E441AB48D039E68003E956918073CC2A52760E7E854A4E01CFF01AB938FC", Binascii::hexlify(mock_artic->m_last_packet).c_str());
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with long packet
	fake_config_store->set_battery_level(40);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("68E381A949C039FE03800003FC48B6", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_rtc->settime(3600);

	// Setup zone config
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	fake_config_store->write_param(ParamID::ZONE_TYPE, zone_type);
	bool zone_enable_out_of_zone_detection_mode = true;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	bool zone_enable_activation_date = false;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_ACTIVATION_DATE, zone_enable_activation_date);
	unsigned int zone_delta_arg_loc_argos_seconds = 3600;
	fake_config_store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	BaseArgosDepthPile zone_argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DEPTH_PILE, zone_argos_depth_pile);
	BaseArgosPower zone_argos_power = BaseArgosPower::POWER_200_MW;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_POWER, zone_argos_power);
	unsigned int zone_argos_time_repetition_seconds = 240;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_REPETITION_SECONDS, zone_argos_time_repetition_seconds);
	BaseArgosMode zone_argos_mode = BaseArgosMode::LEGACY;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_MODE, zone_argos_mode);
	unsigned int zone_argos_duty_cycle = 0xFFFFFFU;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DUTY_CYCLE, zone_argos_duty_cycle);
	unsigned int zone_hdop_filter_threshold = 2;
	fake_config_store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);
	unsigned int zone_gnss_acquisition_timeout_seconds = 240;
	fake_config_store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	double zone_center_longitude_x = -123.3925;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = -48.8752;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_radius_m = 100;
	fake_config_store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;	// Setup zone config
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.header.seconds = 0;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.sec = 0;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)zone_argos_power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with short packet
	fake_config_store->set_battery_level(100);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("79E381A949C039FE03A00003C00552", Binascii::hexlify(mock_artic->m_last_packet).c_str());

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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_rtc->settime(3600);

	// Setup zone config
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	fake_config_store->write_param(ParamID::ZONE_TYPE, zone_type);
	bool zone_enable_out_of_zone_detection_mode = true;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	bool zone_enable_activation_date = false;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_ACTIVATION_DATE, zone_enable_activation_date);
	unsigned int zone_delta_arg_loc_argos_seconds = 3600;
	fake_config_store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	BaseArgosDepthPile zone_argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_4;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DEPTH_PILE, zone_argos_depth_pile);
	BaseArgosPower zone_argos_power = BaseArgosPower::POWER_200_MW;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_POWER, zone_argos_power);
	unsigned int zone_argos_time_repetition_seconds = 240;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_REPETITION_SECONDS, zone_argos_time_repetition_seconds);
	BaseArgosMode zone_argos_mode = BaseArgosMode::LEGACY;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_MODE, zone_argos_mode);
	unsigned int zone_argos_duty_cycle = 0xFFFFFFU;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DUTY_CYCLE, zone_argos_duty_cycle);
	unsigned int zone_hdop_filter_threshold = 2;
	fake_config_store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);
	unsigned int zone_gnss_acquisition_timeout_seconds = 240;
	fake_config_store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	double zone_center_longitude_x = -123.3925;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = -48.8752;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_radius_m = 100;
	fake_config_store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 14;
	gps_entry.header.minutes = 1;
	gps_entry.header.seconds = 0;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 14;
	gps_entry.info.min = 1;
	gps_entry.info.sec = 0;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 15;
	gps_entry.header.minutes = 1;
	gps_entry.header.seconds = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 1;
	gps_entry.info.sec = 0;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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
	gps_entry.header.seconds = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 16;
	gps_entry.info.min = 1;
	gps_entry.info.sec = 0;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8576;
	gps_entry.info.lat = -35.4584;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;  // km/hr -> mm/s
	gps_entry.info.headMot = 0;
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	gps_entry.header.year = 2020;
	gps_entry.header.month = 4;
	gps_entry.header.day = 28;
	gps_entry.header.hours = 17;
	gps_entry.header.minutes = 1;
	gps_entry.header.seconds = 0;
	gps_entry.info.batt_voltage = 3000;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 28;
	gps_entry.info.hour = 17;
	gps_entry.info.min = 1;
	gps_entry.info.sec = 0;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)zone_argos_power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// Should now run with long packet
	fake_config_store->set_battery_level(100);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	STRCMP_EQUAL("38E441AB48D039E68023C956918073CC2A52760E7E854A4E01CFF08683679B", Binascii::hexlify(mock_artic->m_last_packet).c_str());
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
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	// This log entry should not trigger a time sync burst
	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();
	system_scheduler->run();
}


TEST(ArgosScheduler, LegacyModeSchedulingShortPacketInfiniteBurst)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	unsigned int ntry_per_message = 0;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;
	unsigned int t = 0;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	for (unsigned int i = 0; i < 10; i++) {
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		while (!system_scheduler->run());
		STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());
		t += 60;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
		tx_counter++;
	}
}


TEST(ArgosScheduler, LegacyModeSchedulingShortPacketInfiniteBurstWithTimeSyncro)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_16;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	unsigned int ntry_per_message = 0;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = true;
	bool tx_jitter_en = false;
	unsigned int t = 0;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	gps_entry.info.onTime = 0;
	gps_entry.header.minutes = gps_entry.info.min;
	gps_entry.header.hours = gps_entry.info.hour;
	gps_entry.header.day = gps_entry.info.day;
	gps_entry.header.month = gps_entry.info.month;
	gps_entry.header.year = gps_entry.info.year;

	for (unsigned int i = 0; i < 10; i++) {
		printf("i=%u\n", i);

		fake_log->write(&gps_entry);
		argos_sched->notify_sensor_log_update();

		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);

		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		if (i == 0)
			mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		else
			mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

		system_scheduler->run();

		if (i == 0)
			STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());
		if (i == 1)
			STRCMP_EQUAL("693BC63EA7FC011BE00FC27D4FF80237FFFFFFFFFFFFFFFFFFFFFFA6BB7041", Binascii::hexlify(mock_artic->m_last_packet).c_str());
		if (i == 2)
			STRCMP_EQUAL("C23BC63EA7FC011BE00FC27D4FF80237CFA9FF0046FFFFFFFFFFFFAF97B7D0", Binascii::hexlify(mock_artic->m_last_packet).c_str());
		if (i >= 3)
			STRCMP_EQUAL("393BC63EA7FC011BE00FC27D4FF80237CFA9FF0046F9F53FE008DFFF84080A", Binascii::hexlify(mock_artic->m_last_packet).c_str());

		t += 60;
	}
}


TEST(ArgosScheduler, SchedulingNonPrepassTxJitter)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	unsigned int ntry_per_message = 0;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = true;
	uint64_t t = 0;

	fake_rtc->settime((t+500)/1000);
	fake_timer->set_counter(t);

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	t += argos_sched->get_next_schedule();
	fake_rtc->settime((t + 500)/1000);
	fake_timer->set_counter(t);

	for (unsigned int i = 0; i < 10; i++) {
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		while (!system_scheduler->run());
		STRCMP_EQUAL("343BC63EA7FC011BE000000FC2B06C", Binascii::hexlify(mock_artic->m_last_packet).c_str());
		t += argos_sched->get_next_schedule();
		fake_rtc->settime((t + 500)/1000);
		fake_timer->set_counter(t);
		tx_counter++;
	}
}

TEST(ArgosScheduler, PrepassSchedulingShortPacketTXJitter)
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
	unsigned int argos_hexid = 2;
	unsigned int lb_threshold = 0U;
	unsigned int dloc_arg_nom = 60*60U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = true;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);

	// Sample configuration provided with prepass library V3.4
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);

	uint64_t t = 1580083200UL * 1000UL;
	fake_rtc->settime((t + 500)/1000);
	fake_timer->set_counter(t);
	argos_sched->notify_sensor_log_update();

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += argos_sched->get_next_schedule();
	fake_rtc->settime((t + 500) / 1000);
	fake_timer->set_counter(t);
	system_scheduler->run();

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	for (unsigned int i = 0; i < 10; i++) {
		printf("i=%u\n", i);
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

		t += argos_sched->get_next_schedule();
		fake_rtc->settime((t + 500)/1000);
		fake_timer->set_counter(t);
		system_scheduler->run();
	}
}


TEST(ArgosScheduler, OutOfZoneModeChangeLegacy)
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
	bool time_sync_burst_en = false;
	bool tx_jitter_en = false;

	// Sample configuration provided with prepass library V3.4
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

	fake_config_store->write_pass_predict(pass_predict);

	// Setup zone config
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	fake_config_store->write_param(ParamID::ZONE_TYPE, zone_type);
	bool zone_enable_out_of_zone_detection_mode = true;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	bool zone_enable_activation_date = false;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_ACTIVATION_DATE, zone_enable_activation_date);
	unsigned int zone_delta_arg_loc_argos_seconds = 3600;
	fake_config_store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	BaseArgosDepthPile zone_argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DEPTH_PILE, zone_argos_depth_pile);
	BaseArgosPower zone_argos_power = BaseArgosPower::POWER_200_MW;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_POWER, zone_argos_power);
	unsigned int zone_argos_time_repetition_seconds = 60;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_REPETITION_SECONDS, zone_argos_time_repetition_seconds);
	BaseArgosMode zone_argos_mode = BaseArgosMode::PASS_PREDICTION;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_MODE, zone_argos_mode);
	unsigned int zone_argos_duty_cycle = 0xFFFFFFU;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DUTY_CYCLE, zone_argos_duty_cycle);
	unsigned int zone_hdop_filter_threshold = 2;
	fake_config_store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);
	unsigned int zone_gnss_acquisition_timeout_seconds = 240;
	fake_config_store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	double zone_center_longitude_x = -123.3925;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = -48.8752;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_radius_m = 500;
	fake_config_store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	uint64_t t = 1631195464;
	fake_rtc->settime(t);
	fake_timer->set_counter(t * 1000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	// This log entry should be inside the zone

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -123.3925;
	gps_entry.info.lat = -48.8752;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	fake_config_store->notify_gps_location(gps_entry);
	argos_sched->notify_sensor_log_update();

	t += 56;
	fake_rtc->settime(t);
	fake_timer->set_counter(t * 1000);
	while(!system_scheduler->run());

	// This log entry should not trigger an OUT_OF_ZONE

	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	fake_config_store->notify_gps_location(gps_entry);
	argos_sched->notify_sensor_log_update();

	fake_log->write(&gps_entry);
	fake_config_store->notify_gps_location(gps_entry);
	argos_sched->notify_sensor_log_update();

	t += 6184;
	fake_rtc->settime(t);
	fake_timer->set_counter(t * 1000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)zone_argos_power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	while(!system_scheduler->run());

}


TEST(ArgosScheduler, OutOfZoneModeChangePrepass)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool time_sync_burst_en = false;
	bool tx_jitter_en = false;

	// Sample configuration provided with prepass library V3.4
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

	fake_config_store->write_pass_predict(pass_predict);

	// Setup zone config
	BaseZoneType zone_type = BaseZoneType::CIRCLE;
	fake_config_store->write_param(ParamID::ZONE_TYPE, zone_type);
	bool zone_enable_out_of_zone_detection_mode = true;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_OUT_OF_ZONE_DETECTION_MODE, zone_enable_out_of_zone_detection_mode);
	bool zone_enable_activation_date = false;
	fake_config_store->write_param(ParamID::ZONE_ENABLE_ACTIVATION_DATE, zone_enable_activation_date);
	unsigned int zone_delta_arg_loc_argos_seconds = 3600;
	fake_config_store->write_param(ParamID::ZONE_GNSS_DELTA_ARG_LOC_ARGOS_SECONDS, zone_delta_arg_loc_argos_seconds);
	BaseArgosDepthPile zone_argos_depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DEPTH_PILE, zone_argos_depth_pile);
	BaseArgosPower zone_argos_power = BaseArgosPower::POWER_200_MW;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_POWER, zone_argos_power);
	unsigned int zone_argos_time_repetition_seconds = 60;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_REPETITION_SECONDS, zone_argos_time_repetition_seconds);
	BaseArgosMode zone_argos_mode = BaseArgosMode::LEGACY;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_MODE, zone_argos_mode);
	unsigned int zone_argos_duty_cycle = 0xFFFFFFU;
	fake_config_store->write_param(ParamID::ZONE_ARGOS_DUTY_CYCLE, zone_argos_duty_cycle);
	unsigned int zone_hdop_filter_threshold = 2;
	fake_config_store->write_param(ParamID::ZONE_GNSS_HDOPFILT_THR, zone_hdop_filter_threshold);
	unsigned int zone_gnss_acquisition_timeout_seconds = 240;
	fake_config_store->write_param(ParamID::ZONE_GNSS_ACQ_TIMEOUT, zone_gnss_acquisition_timeout_seconds);
	double zone_center_longitude_x = -123.3925;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LONGITUDE, zone_center_longitude_x);
	double zone_center_latitude_y = -48.8752;
	fake_config_store->write_param(ParamID::ZONE_CENTER_LATITUDE, zone_center_latitude_y);
	unsigned int zone_radius_m = 500;
	fake_config_store->write_param(ParamID::ZONE_RADIUS, zone_radius_m);

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	uint64_t t = 1631195464;
	fake_rtc->settime(t);
	fake_timer->set_counter(t * 1000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	// This log entry should be inside the zone

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -123.3925;
	gps_entry.info.lat = -48.8752;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	fake_config_store->notify_gps_location(gps_entry);
	argos_sched->notify_sensor_log_update();

	t += 56;
	fake_rtc->settime(t);
	fake_timer->set_counter(t * 1000);
	while(!system_scheduler->run());

	// This log entry should not trigger an OUT_OF_ZONE

	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	fake_config_store->notify_gps_location(gps_entry);
	argos_sched->notify_sensor_log_update();

	fake_log->write(&gps_entry);
	fake_config_store->notify_gps_location(gps_entry);
	argos_sched->notify_sensor_log_update();

	t += 60;
	fake_rtc->settime(t);
	fake_timer->set_counter(t * 1000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)zone_argos_power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	system_scheduler->run();
}


TEST(ArgosScheduler, LegacyModeSchedulingShortPacketInfiniteBurstWithNonZeroSensorLog)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	unsigned int ntry_per_message = 0;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;
	unsigned int t = 0;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	GPSLogEntry gps_entry;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = 2020;
	gps_entry.info.month = 4;
	gps_entry.info.day = 7;
	gps_entry.info.hour = 15;
	gps_entry.info.min = 6;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	// Create some existing log entries -- these should not be transmitted as they a prior
	// to the scheduler running and are not notified
	fake_log->write(&gps_entry);
	fake_log->write(&gps_entry);
	fake_log->write(&gps_entry);

	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->start();

	// Modify GPS coordinates for "new" log entry
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 30.1;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	for (unsigned int i = 0; i < 10; i++) {
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		while (!system_scheduler->run());
		STRCMP_EQUAL("F43BC624BE44011BE000000FD0D8A6", Binascii::hexlify(mock_artic->m_last_packet).c_str());
		t += 60;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
		tx_counter++;
	}
}

TEST(ArgosScheduler, LegacyModeSchedulingShortPacketInfiniteBurstConfirmDepthPile16)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_16;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0U;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::LEGACY;
	unsigned int ntry_per_message = 0;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 60;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;
	uint16_t year = 2021;
	uint8_t  month = 9, day = 21, hour = 8, minute = 58, second = 0;
	std::time_t t;

	t = convert_epochtime(year, month, day, hour, minute, second);

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);

	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->start();

	GPSLogEntry gps_entry;
	gps_entry.header.year = year;
	gps_entry.header.month = month;
	gps_entry.header.day = day;
	gps_entry.header.hours = hour;
	gps_entry.header.minutes = minute;
	gps_entry.header.seconds = second;
	gps_entry.info.onTime = 0;
	gps_entry.info.batt_voltage = 3960;
	gps_entry.info.year = year;
	gps_entry.info.month = month;
	gps_entry.info.day = day;
	gps_entry.info.hour = hour;
	gps_entry.info.min = minute;
	gps_entry.info.sec = second;
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = -0.2271;
	gps_entry.info.lat = 51.3279;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 0;
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;

	fake_log->write(&gps_entry);
	argos_sched->notify_sensor_log_update();

	unsigned int k = 0;
	for (unsigned int i = 0; i < 1000; i++) {
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", (k<10) ? 120 : 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		while (!system_scheduler->run());

		t += 60;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
		tx_counter++;
		k++;

		if ((k % 10) == 0) {
			convert_datetime_to_epoch(t, year, month, day, hour, minute, second);
			printf(">>> Logging: %u/%u/%u %02u:%02u:%02u\n", (unsigned int)year, (unsigned int)month, (unsigned int)day, (unsigned int)hour, (unsigned int)minute, (unsigned int)second);
			gps_entry.header.year = year;
			gps_entry.header.day = day;
			gps_entry.header.month = month;
			gps_entry.header.hours = hour;
			gps_entry.header.minutes = minute;
			gps_entry.header.seconds = second;
			gps_entry.info.batt_voltage = 3960;
			gps_entry.info.year = year;
			gps_entry.info.month = month;
			gps_entry.info.day = day;
			gps_entry.info.hour = hour;
			gps_entry.info.min = minute;
			gps_entry.info.sec = second;
			fake_log->write(&gps_entry);
			argos_sched->notify_sensor_log_update();
		}

	}
}

TEST(ArgosScheduler, TestDownlinkReceive)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Not used
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 20;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 300;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	unsigned int dloc_arg_nom = 60*60U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;
	unsigned int rx_aop_update_period = 0U;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	// TX shall be scheduled first

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// RX shall be immediately after TX is done
	mock().expectOneCall("set_rx_mode").onObject(argos_sched).ignoreOtherParameters();

	t += 24050;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
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

	t += 100;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	// Last required RX packet should trigger power off
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(100000);

	x = "00000BE400D4849848904D8D33269EAF627189023C5658A5";
	mock_artic->inject_rx_packet(x, x.size() * 8);

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
	CHECK_EQUAL(100, rx_time);
}


TEST(ArgosScheduler, TestDownlinkWithAOPUpdatePeriod)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Not used
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 20;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 300;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	unsigned int dloc_arg_nom = 60*60U;
	unsigned int rx_aop_update_period = 1U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
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

	// TX shall be scheduled first

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// RX shall be immediately after TX is done
	mock().expectOneCall("set_rx_mode").onObject(argos_sched).ignoreOtherParameters();

	t += 24050;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();

	std::string x = "00000C77007A5C900B7C500800C00D4C4224";
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

	// Last required RX packet should trigger power off
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);

	x = "00000BE400D4849848904D8D33269EAF627189023C5658A5";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	// Now check that the records have been updated
	pass_predict = configuration_store->read_pass_predict();
	CHECK_EQUAL(8, pass_predict.num_records);

#if 0
	// Dump the AOP records
	for (unsigned int i = 0; i < pass_predict.num_records; i++) {
		printf("AOP[%u]: satHexId=%01x\n", i,
				pass_predict.records[i].satHexId);
		printf("AOP[%u]: bulletin=%02u/%02u/%04u\n", i,
				pass_predict.records[i].bulletin.day,
				pass_predict.records[i].bulletin.month,
				pass_predict.records[i].bulletin.year);
		printf("AOP[%u]: downlinkStatus=%01x\n", i,
				pass_predict.records[i].downlinkStatus);
		printf("AOP[%u]: uplinkStatus=%01x\n", i,
				pass_predict.records[i].uplinkStatus);
		printf("AOP[%u]: semiMajorAxisKm=%f\n", i,
				(double)pass_predict.records[i].semiMajorAxisKm);
		printf("AOP[%u]: inclinationDeg=%f\n", i,
				(double)pass_predict.records[i].inclinationDeg);
		printf("AOP[%u]: ascNodeLongitudeDeg=%f\n", i,
				(double)pass_predict.records[i].ascNodeLongitudeDeg);
		printf("AOP[%u]: ascNodeDriftDeg=%f\n", i,
				(double)pass_predict.records[i].ascNodeDriftDeg);
		printf("AOP[%u]: orbitPeriodMin=%f\n", i,
				(double)pass_predict.records[i].orbitPeriodMin);
		printf("AOP[%u]: semiMajorAxisDriftMeterPerDay=%f\n", i,
				(double)pass_predict.records[i].semiMajorAxisDriftMeterPerDay);
	}
#endif

	// Check last AOP date
	std::time_t last_aop_update;
	last_aop_update = configuration_store->read_param<std::time_t>(ParamID::ARGOS_AOP_DATE);
	CHECK_EQUAL(t, last_aop_update);


	t = 1632996203U;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	mock().expectOneCall("power_off_immediate").onObject(argos_sched);
	argos_sched->stop();
	argos_sched->start();

	argos_sched->notify_sensor_log_update();

	// TX shall be scheduled first

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// RX shall be immediately after TX is done
	mock().expectOneCall("set_rx_mode").onObject(argos_sched).ignoreOtherParameters();

	t += 20151;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();

	x = "00000C75007A5C900B7C500800C00D4CC8E2";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "000005F5006601A58900B78C00D4806E1C";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000D450064849C88C1C0CC1DE9F73855F56523606D0546";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE50084849C88D151A8A0D3EEDA9371A8084D856774";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE500C4849C88C1C564AE7BECDADB737F0655183984";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	// Should go idle before next TX when RX is already active
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// RX shall be immediately after TX is done
	mock().expectOneCall("set_rx_mode").onObject(argos_sched).ignoreOtherParameters();

	t += 300;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();

	x = "00000BE500A4849C885554E84C0D26C6B2FBB200392927AB";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE50094849C88C48C68D87528C6CAFC6200429904BF";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE500B4849C8888CC2A55D528C6BAFC16004364E166";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE50054849C88CC4E2475C5CCBC92BFE5014179C603";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);

	x = "00000BE500D4849C8855D00E8A369EAF62719C023C5EAE6A";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += 300;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += 300;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}

TEST(ArgosScheduler, GNSSOffLegacySchedulingDopplerPacket)
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
	bool tx_jitter_en = false;
	bool gnss_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);

	fake_rtc->settime(0);
	fake_timer->set_counter(0);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 24).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	while (!system_scheduler->run());

	STRCMP_EQUAL("0500B4", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(1, tx_counter);

	fake_rtc->settime(60);
	fake_timer->set_counter(60000);
	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 24).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);

	while (!system_scheduler->run());

	STRCMP_EQUAL("0500B4", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	tx_counter = fake_config_store->read_param<unsigned int>(ParamID::TX_COUNTER);
	CHECK_EQUAL(2, tx_counter);

	// Make sure configuration store has been saved for each transmission
	CHECK_EQUAL(2, fake_config_store->get_saved_count());
}

TEST(ArgosScheduler, GNSSOffDutyCycleSchedulingDopplerPacket)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0xAAAAAAU;
	double frequency = 900.11;
	BaseArgosMode mode = BaseArgosMode::DUTY_CYCLE;
	unsigned int ntry_per_message = 1;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 600;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;
	bool gnss_en = false;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::GNSS_EN, gnss_en);

	std::time_t t = 0;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->start();

	for (unsigned int i = 0; i < 6; i++) {
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 24).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		while (!system_scheduler->run());
		STRCMP_EQUAL("0500B4", Binascii::hexlify(mock_artic->m_last_packet).c_str());

		t += 600;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
	}

	t += 3600;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	for (unsigned int i = 0; i < 6; i++) {
		mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
		mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
		mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
		mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
		mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 24).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
		mock().expectOneCall("power_off").onObject(argos_sched);
		mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
		while (!system_scheduler->run());
		STRCMP_EQUAL("0500B4", Binascii::hexlify(mock_artic->m_last_packet).c_str());

		t += 600;
		fake_rtc->settime(t);
		fake_timer->set_counter(t*1000);
	}

}


TEST(ArgosScheduler, TestAOPUpdateWithOutOfServiceSatellite)
{
	BaseArgosDepthPile depth_pile = BaseArgosDepthPile::DEPTH_PILE_1;
	unsigned int dry_time_before_tx = 10;
	unsigned int duty_cycle = 0x0U; // Not used
	double frequency = 900.22;
	BaseArgosMode mode = BaseArgosMode::PASS_PREDICTION;
	unsigned int ntry_per_message = 20;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	unsigned int tr_nom = 300;
	unsigned int tx_counter = 0;
	unsigned int argos_hexid = 0x01234567U;
	unsigned int lb_threshold = 0U;
	unsigned int dloc_arg_nom = 60*60U;
	bool lb_en = false;
	bool underwater_en = false;
	bool sync_burst_en = false;
	bool tx_jitter_en = false;
	unsigned int rx_aop_update_period = 0U;

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
	fake_config_store->write_param(ParamID::ARGOS_TX_JITTER_EN, tx_jitter_en);
	fake_config_store->write_param(ParamID::DLOC_ARG_NOM, dloc_arg_nom);
	fake_config_store->write_param(ParamID::ARGOS_RX_AOP_UPDATE_PERIOD, rx_aop_update_period);

	// Sample configuration provided with prepass library V3.4
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

	// Write ARGOS_AOP_DATE
	std::time_t last_aop = 0;
	fake_config_store->write_param(ParamID::ARGOS_AOP_DATE, last_aop);

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
	gps_entry.info.schedTime = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
	gps_entry.info.valid = 1;
	gps_entry.info.lon = 11.8768;
	gps_entry.info.lat = -33.8232;
	gps_entry.info.hMSL = 0;
	gps_entry.info.gSpeed = 8056;  // mm/s
	gps_entry.info.headMot = 0;
	gps_entry.info.fixType = 3;
	fake_log->write(&gps_entry);

	std::time_t t = 1580099520;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	argos_sched->notify_sensor_log_update();

	// TX shall be scheduled first

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	// RX shall be immediately after TX is done
	mock().expectOneCall("set_rx_mode").onObject(argos_sched).ignoreOtherParameters();

	t += 10;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	while (!system_scheduler->run());

	std::string x;
	x = "00000BE500D484C1888D4DCD616E9EAF627186023C44A609";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE5005484C1888C88A64955CCBC9ABFF8094138C713";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "000005F7006601A07900B78C00D4800FD0";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000C77007A03900B7C500800C00D4CF219";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE700B484C1C85648AB14D528C6C2FC2F004329885B";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE700D484C1C881044E56FE9EAF5A7157023C3D5579";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000E17082021308215302537CB29";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE7009484C1C89201E9971528C6B2FBF90042691CC8";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "000005F4006601A07900B78C00D4800A4F";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000D44006484C1C8848C8DC791FB3825F433336052F78A";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000BE400C484C1C89591A52A3BECDAD3734F0654E17BE3";
	mock_artic->inject_rx_packet(x, x.size() * 8);
	x = "00000E14082021309060927978E50A";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	t += 100;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);

	// Last required RX packet should trigger power off
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(100000);

	x = "00000BE4008484C1C8520540573BEEDA837142094D5A0E56";
	mock_artic->inject_rx_packet(x, x.size() * 8);

	// Now check that the records have been updated
	pass_predict = configuration_store->read_pass_predict();
	CHECK_EQUAL(8, pass_predict.num_records);

	// Check last AOP date
	std::time_t last_aop_update;
	last_aop_update = configuration_store->read_param<std::time_t>(ParamID::ARGOS_AOP_DATE);
	CHECK_EQUAL(t, last_aop_update);

	// Check RX counter
	unsigned int rx_counter = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_COUNTER);
	CHECK_EQUAL(13, rx_counter);

	// Check RX time
	unsigned int rx_time = configuration_store->read_param<unsigned int>(ParamID::ARGOS_RX_TIME);
	CHECK_EQUAL(100, rx_time);
}


TEST(ArgosScheduler, CertificationTXShortPacketA2MinimumRepetition)
{
	bool cert_tx_enable = true;
	BaseArgosModulation cert_tx_modulation = BaseArgosModulation::A2;
	unsigned int cert_tx_repetition = 2;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	double frequency = 900.11;
	unsigned int argos_hexid = 0x01234567U;
	std::string cert_tx_payload = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s;

	std::time_t t = 0;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::CERT_TX_ENABLE, cert_tx_enable);
	fake_config_store->write_param(ParamID::CERT_TX_MODULATION, cert_tx_modulation);
	fake_config_store->write_param(ParamID::CERT_TX_REPETITION, cert_tx_repetition);
	fake_config_store->write_param(ParamID::CERT_TX_PAYLOAD, cert_tx_payload);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	system_scheduler->run();

	STRCMP_EQUAL("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += cert_tx_repetition;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}

TEST(ArgosScheduler, CertificationTXShortPacketA2)
{
	bool cert_tx_enable = true;
	BaseArgosModulation cert_tx_modulation = BaseArgosModulation::A2;
	unsigned int cert_tx_repetition = 30;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	double frequency = 900.11;
	unsigned int argos_hexid = 0x01234567U;
	std::string cert_tx_payload = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s;

	std::time_t t = 0;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::CERT_TX_ENABLE, cert_tx_enable);
	fake_config_store->write_param(ParamID::CERT_TX_MODULATION, cert_tx_modulation);
	fake_config_store->write_param(ParamID::CERT_TX_REPETITION, cert_tx_repetition);
	fake_config_store->write_param(ParamID::CERT_TX_PAYLOAD, cert_tx_payload);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	system_scheduler->run();

	STRCMP_EQUAL("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += cert_tx_repetition;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}

TEST(ArgosScheduler, CertificationTXShortPacketA3)
{
	bool cert_tx_enable = true;
	BaseArgosModulation cert_tx_modulation = BaseArgosModulation::A3;
	unsigned int cert_tx_repetition = 30;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	double frequency = 900.11;
	unsigned int argos_hexid = 0x01234567U;
	std::string cert_tx_payload = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s;

	std::time_t t = 0;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::CERT_TX_ENABLE, cert_tx_enable);
	fake_config_store->write_param(ParamID::CERT_TX_MODULATION, cert_tx_modulation);
	fake_config_store->write_param(ParamID::CERT_TX_REPETITION, cert_tx_repetition);
	fake_config_store->write_param(ParamID::CERT_TX_PAYLOAD, cert_tx_payload);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	system_scheduler->run();

	STRCMP_EQUAL("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += cert_tx_repetition;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}


TEST(ArgosScheduler, CertificationTXLongPacketA3)
{
	bool cert_tx_enable = true;
	BaseArgosModulation cert_tx_modulation = BaseArgosModulation::A3;
	unsigned int cert_tx_repetition = 30;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	double frequency = 900.11;
	unsigned int argos_hexid = 0x01234567U;
	std::string cert_tx_payload = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

	std::time_t t = 0;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::CERT_TX_ENABLE, cert_tx_enable);
	fake_config_store->write_param(ParamID::CERT_TX_MODULATION, cert_tx_modulation);
	fake_config_store->write_param(ParamID::CERT_TX_REPETITION, cert_tx_repetition);
	fake_config_store->write_param(ParamID::CERT_TX_PAYLOAD, cert_tx_payload);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	system_scheduler->run();

	STRCMP_EQUAL("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += cert_tx_repetition;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}


TEST(ArgosScheduler, CertificationTXLongPacketA3PayloadResized)
{
	bool cert_tx_enable = true;
	BaseArgosModulation cert_tx_modulation = BaseArgosModulation::A3;
	unsigned int cert_tx_repetition = 30;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	double frequency = 900.11;
	unsigned int argos_hexid = 0x01234567U;
	std::string cert_tx_payload = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

	std::time_t t = 0;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::CERT_TX_ENABLE, cert_tx_enable);
	fake_config_store->write_param(ParamID::CERT_TX_MODULATION, cert_tx_modulation);
	fake_config_store->write_param(ParamID::CERT_TX_REPETITION, cert_tx_repetition);
	fake_config_store->write_param(ParamID::CERT_TX_PAYLOAD, cert_tx_payload);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	system_scheduler->run();

	STRCMP_EQUAL("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 248).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_3);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += cert_tx_repetition;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}


TEST(ArgosScheduler, CertificationTXShortPacketA4FallsBackToA2)
{
	bool cert_tx_enable = true;
	BaseArgosModulation cert_tx_modulation = BaseArgosModulation::A4;
	unsigned int cert_tx_repetition = 30;
	BaseArgosPower power = BaseArgosPower::POWER_500_MW;
	double frequency = 900.11;
	unsigned int argos_hexid = 0x01234567U;
	std::string cert_tx_payload = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s;

	std::time_t t = 0;

	fake_config_store->write_param(ParamID::ARGOS_FREQ, frequency);
	fake_config_store->write_param(ParamID::ARGOS_POWER, power);
	fake_config_store->write_param(ParamID::ARGOS_HEXID, argos_hexid);
	fake_config_store->write_param(ParamID::CERT_TX_ENABLE, cert_tx_enable);
	fake_config_store->write_param(ParamID::CERT_TX_MODULATION, cert_tx_modulation);
	fake_config_store->write_param(ParamID::CERT_TX_REPETITION, cert_tx_repetition);
	fake_config_store->write_param(ParamID::CERT_TX_PAYLOAD, cert_tx_payload);

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);
	argos_sched->start();

	system_scheduler->run();

	STRCMP_EQUAL("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", Binascii::hexlify(mock_artic->m_last_packet).c_str());

	mock().expectOneCall("power_on").onObject(argos_sched).withUnsignedIntParameter("argos_id", (unsigned int)argos_hexid);
	mock().expectOneCall("power_off").onObject(argos_sched);
	mock().expectOneCall("get_rx_time_on").onObject(argos_sched).andReturnValue(0);
	mock().expectOneCall("set_frequency").onObject(argos_sched).withDoubleParameter("freq", frequency);
	mock().expectOneCall("set_tx_power").onObject(argos_sched).withUnsignedIntParameter("power", (unsigned int)power);
	mock().expectOneCall("set_tcxo_warmup_time").onObject(argos_sched).withUnsignedIntParameter("time", txco_warmup);
	mock().expectOneCall("send_packet").onObject(argos_sched).withUnsignedIntParameter("payload_bits", 120).withUnsignedIntParameter("mode", (unsigned int)ArgosMode::ARGOS_2);
	mock().expectOneCall("get_voltage").onObject(battery_monitor).andReturnValue(4500);

	t += cert_tx_repetition;
	fake_rtc->settime(t);
	fake_timer->set_counter(t*1000);
	system_scheduler->run();
}
