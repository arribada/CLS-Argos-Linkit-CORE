#pragma once

#include "sensor.hpp"
#include "config_store.hpp"
#include "logger.hpp"
#include "messages.hpp"

extern ConfigurationStore *configuration_store;

struct __attribute__((packed)) CDTLogEntry {
	LogHeader header;
	union {
		struct {
			double conductivity;
			double depth;
			double temperature;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

enum class CDTSensorPort : unsigned int {
	CONDUCTIVITY,
	DEPTH,
	TEMPERATURE
};

class CDTLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,conductivity,depth,temperature\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const CDTLogEntry *log = (const CDTLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(log->header.year, log->header.month, log->header.day, log->header.hours, log->header.minutes, log->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f,%f,%f\r\n",
				d1,
				log->conductivity,
				log->depth,
				log->temperature);
		return std::string(entry);
	}
};


class CDTSensor : public Sensor {
public:
	CDTSensor(Logger *logger) : Sensor(ServiceIdentifier::CDT_SENSOR, "CDT", logger) {}
	virtual ~CDTSensor() {}

private:
	virtual void calibrate(double value, unsigned int offset) = 0;
	virtual double read(unsigned int port = 0) = 0;

	void read_and_populate_log_entry(LogEntry *e) override {
		CDTLogEntry *log = (CDTLogEntry *)e;
		log->conductivity = read((unsigned int)CDTSensorPort::CONDUCTIVITY);
		log->depth = read((unsigned int)CDTSensorPort::DEPTH);
		log->temperature = read((unsigned int)CDTSensorPort::TEMPERATURE);
		service_set_log_header_time(log->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = configuration_store->read_param<bool>(ParamID::CDT_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * configuration_store->read_param<unsigned int>(ParamID::CDT_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}
	void service_initiate() override {
		ServiceEventData data;
		LogEntry e;
		read_and_populate_log_entry(&e);
		service_complete(&data, &e);
	}
	bool service_cancel() override { return false; }
	unsigned int service_next_timeout() override { return 0; }
	bool service_is_triggered_on_surfaced() override { return false; }
	bool service_is_usable_underwater() override { return true; }
};
