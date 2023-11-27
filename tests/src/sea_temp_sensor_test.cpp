#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include <iostream>

#include "../../core/services/sea_temp_sensor_service.hpp"
#include "mock_sensor.hpp"
#include "fake_config_store.hpp"
#include "fake_logger.hpp"
#include "fake_rtc.hpp"
#include "fake_timer.hpp"


extern Timer *system_timer;
extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;
extern RTC *rtc;


TEST_GROUP(SeaTempSensor)
{
	FakeConfigurationStore *fake_config_store;
	FakeTimer *fake_timer;
	FakeLog *fake_logger;
	Logger *logger;
	FakeRTC *fake_rtc;

	void setup() {
		fake_timer = new FakeTimer;
		system_timer = fake_timer;
		fake_rtc = new FakeRTC;
		rtc = fake_rtc;
		fake_config_store = new FakeConfigurationStore;
		configuration_store = fake_config_store;
		fake_logger = new FakeLog;
		logger = fake_logger;
		fake_logger->create();
		system_scheduler = new Scheduler(system_timer);
		configuration_store->init();
	}

	void teardown() {
		delete system_scheduler;
		delete fake_timer;
		delete fake_config_store;
		delete fake_logger;
		delete fake_rtc;
	}

	void notify_gnss_active() {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_ACTIVE,
		e.event_source = ServiceIdentifier::GNSS_SENSOR;
		e.event_originator_unique_id = 0x12345678;
		ServiceManager::notify_peer_event(e);
	}

	void notify_gnss_inactive() {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_INACTIVE,
		e.event_source = ServiceIdentifier::GNSS_SENSOR;
		e.event_originator_unique_id = 0x12345678;
		ServiceManager::notify_peer_event(e);
	}
};


TEST(SeaTempSensor, SensorDisabled)
{
	MockSensor drv;
	SeaTempSensorService s(drv, logger);
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int period = 10;
	bool sensor_en = false;

	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE, sensor_en);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_PERIODIC, period);

	s.start([&num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			num_callbacks++;
		}
	});

	// Sampling should happen every 10
	for (unsigned int i = 0; i < 5; i++) {
		fake_timer->increment_counter(period*1000);
		system_scheduler->run();
	}

	CHECK_EQUAL(0, num_callbacks);
	CHECK_EQUAL(0, logger->num_entries());

	s.stop();
}


TEST(SeaTempSensor, SchedulingPeriodic)
{
	MockSensor drv;
	SeaTempSensorService s(drv, logger);
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int period = 10;
	bool sensor_en = true;

	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE, sensor_en);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_PERIODIC, period);

	s.start([&num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			num_callbacks++;
		}
	});

	// Sampling should happen every 10
	for (unsigned int i = 0; i < 5; i++) {
		mock().expectOneCall("read").onObject(&drv).withUnsignedIntParameter("port", 0).andReturnValue((double)i);
		fake_timer->increment_counter(period*1000);
		system_scheduler->run();
	}

	CHECK_EQUAL(5, num_callbacks);
	CHECK_EQUAL(5, logger->num_entries());

	// Validate log entries
	for (unsigned int i = 0; i < 5; i++) {
		SeaTempLogEntry e;
		logger->read(&e, i);
		CHECK_EQUAL((double)i, e.sea_temp);
	}

	s.stop();
}


TEST(SeaTempSensor, SchedulingNoPeriodic)
{
	MockSensor drv;
	SeaTempSensorService s(drv, logger);
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int period = 0;
	bool sensor_en = true;

	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE, sensor_en);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_PERIODIC, period);

	s.start([&num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			num_callbacks++;
		}
	});

	// Sampling should happen every 10
	for (unsigned int i = 0; i < 5; i++) {
		fake_timer->increment_counter(period*1000);
		system_scheduler->run();
	}

	CHECK_EQUAL(0, num_callbacks);
	CHECK_EQUAL(0, logger->num_entries());

	s.stop();
}

TEST(SeaTempSensor, SchedulingTxEnableOneShot)
{
	MockSensor drv;
	SeaTempSensorService s(drv, logger);
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int period = 10;
	bool sensor_en = true;
	BaseSensorEnableTxMode mode = BaseSensorEnableTxMode::ONESHOT;

	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE, sensor_en);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_PERIODIC, period);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE_TX_MODE, mode);

	s.start([&num_callbacks](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			num_callbacks++;
		}
	});

	// Sampling is triggered by GNSS
	notify_gnss_active();

	// Sampling should happen once in one-shot mode
	for (unsigned int i = 0; i < 1; i++) {
		mock().expectOneCall("read").onObject(&drv).withUnsignedIntParameter("port", 0).andReturnValue((double)i);
		fake_timer->increment_counter(period*1000);
		system_scheduler->run();
	}

	notify_gnss_inactive();

	CHECK_EQUAL(1, num_callbacks);
	CHECK_EQUAL(1, logger->num_entries());

	// Sampling should happen once in one-shot mode
	for (unsigned int i = 0; i < 1; i++) {
		SeaTempLogEntry e;
		logger->read(&e, i);
		CHECK_EQUAL((double)i, e.sea_temp);
	}

	s.stop();
}


TEST(SeaTempSensor, SchedulingTxEnableMean)
{
	MockSensor drv;
	SeaTempSensorService s(drv, logger);
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int tx_period = 1;
	unsigned int period = 10;
	bool sensor_en = true;
	BaseSensorEnableTxMode mode = BaseSensorEnableTxMode::MEAN;
	ServiceSensorData sensorData;

	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE, sensor_en);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_PERIODIC, period);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE_TX_MODE, mode);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE_TX_SAMPLE_PERIOD, tx_period);

	s.start([&num_callbacks,&sensorData](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			num_callbacks++;
			sensorData = std::get<ServiceSensorData>(event.event_data);
		}
	});

	// Sampling is triggered by GNSS
	notify_gnss_active();

	// Sampling should happen periodically in mean sampling mode
	for (unsigned int i = 0; i < 100; i++) {
		mock().expectOneCall("read").onObject(&drv).withUnsignedIntParameter("port", 0).andReturnValue((double)i);
		fake_timer->increment_counter(period);
		system_scheduler->run();
	}

	notify_gnss_inactive();

	CHECK_EQUAL(1, num_callbacks);
	CHECK_EQUAL(1, logger->num_entries());
	SeaTempLogEntry e;
	logger->read(&e, 0);
	CHECK_EQUAL((double)49.5, e.sea_temp);
	CHECK_EQUAL((double)49.5, sensorData.port[0]);

	s.stop();
}

TEST(SeaTempSensor, SchedulingTxEnableMedian)
{
	MockSensor drv;
	SeaTempSensorService s(drv, logger);
	unsigned int num_callbacks = 0;

	system_timer->start();

	unsigned int tx_period = 1;
	unsigned int period = 10;
	bool sensor_en = true;
	BaseSensorEnableTxMode mode = BaseSensorEnableTxMode::MEDIAN;
	ServiceSensorData sensorData;

	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE, sensor_en);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_PERIODIC, period);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE_TX_MODE, mode);
	configuration_store->write_param(ParamID::SEA_TEMP_SENSOR_ENABLE_TX_SAMPLE_PERIOD, tx_period);

	s.start([&num_callbacks,&sensorData](ServiceEvent &event) {
		if (event.event_type == ServiceEventType::SERVICE_LOG_UPDATED) {
			num_callbacks++;
			sensorData = std::get<ServiceSensorData>(event.event_data);
		}
	});

	// Sampling is triggered by GNSS
	notify_gnss_active();

	// Sampling should happen periodically in mean sampling mode
	for (unsigned int i = 0; i < 100; i++) {
		mock().expectOneCall("read").onObject(&drv).withUnsignedIntParameter("port", 0).andReturnValue((double)i);
		fake_timer->increment_counter(period);
		system_scheduler->run();
	}

	notify_gnss_inactive();

	CHECK_EQUAL(1, num_callbacks);
	CHECK_EQUAL(1, logger->num_entries());
	SeaTempLogEntry e;
	logger->read(&e, 0);
	CHECK_EQUAL((double)50, e.sea_temp);
	CHECK_EQUAL((double)50, sensorData.port[0]);

	s.stop();
}
