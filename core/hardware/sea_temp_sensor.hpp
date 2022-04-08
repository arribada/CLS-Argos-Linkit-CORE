#pragma once

#include "sensor.hpp"
#include "config_store.hpp"
#include "logger.hpp"
#include "messages.hpp"

extern ConfigurationStore *configuration_store;

struct __attribute__((packed)) SeaTempLogEntry {
	LogHeader header;
	union {
		double sea_temp;
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

class SeaTempLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,sea_temp\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const SeaTempLogEntry *log = (const SeaTempLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(log->header.year, log->header.month, log->header.day, log->header.hours, log->header.minutes, log->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f\r\n",
				d1,
				log->lumens);
		return std::string(entry);
	}
};


class SeaTempSensor : public Sensor {
public:
	SeaTempSensor(Logger *logger) : Sensor(ServiceIdentifier::SEA_TEMP_SENSOR, "SEA_TEMP", logger) {}
	virtual ~SeaTempSensor() {}

private:
	virtual void calibrate(double value, unsigned int offset) = 0;
	virtual double read(unsigned int port = 0) = 0;

	void read_and_populate_log_entry(LogEntry *e) override {
		SeaTempLogEntry *log = (SeaTempLogEntry *)e;
		log->sea_temp = read();
		service_set_log_header_time(log->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = configuration_store->read_param<bool>(ParamID::SEA_TEMP_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * configuration_store->read_param<unsigned int>(ParamID::SEA_TEMP_SENSOR_PERIODIC);
		return schedule;
	}
	void service_initiate() override {
		ServiceEventData data;
		LogEntry e;
		read_and_populate_log_entry(&e);
		service_complete(&data, &log);
	}
	bool service_cancel() override { return false; }
	unsigned int service_next_timeout() override { return 0; }
	bool service_is_triggered_on_surfaced() override { return false; }
	bool service_is_usable_underwater() override { return true; }
};