#pragma once

#include "logger.hpp"
#include "messages.hpp"
#include "sensor_service.hpp"
#include "timeutils.hpp"

struct __attribute__((packed)) ALSLogEntry {
	LogHeader header;
	union {
		double lumens;
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

class ALSLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,lumens\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const ALSLogEntry *log = (const ALSLogEntry *)&e;
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


class ALSSensorService : public SensorService {
public:
	ALSSensorService(Sensor& sensor, Logger *logger = nullptr) : SensorService(sensor, ServiceIdentifier::ALS_SENSOR, "ALS", logger) {}

private:
	void read_and_populate_log_entry(LogEntry *e) override {
		ALSLogEntry *log = (ALSLogEntry *)e;
		log->lumens = m_sensor.read();
		service_set_log_header_time(log->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = service_read_param<bool>(ParamID::ALS_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::ALS_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}
	void service_initiate() override {
		ServiceEventData data;
		LogEntry e;
		read_and_populate_log_entry(&e);
		service_complete(&data, &e);
	}
	bool service_cancel() override { return false; }
	bool service_is_usable_underwater() override { return true; }
};
