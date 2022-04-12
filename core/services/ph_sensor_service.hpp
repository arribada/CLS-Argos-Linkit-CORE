#pragma once

#include "sensor_service.hpp"
#include "logger.hpp"
#include "messages.hpp"
#include "timeutils.hpp"

struct __attribute__((packed)) PHLogEntry {
	LogHeader header;
	union {
		double ph;
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};


class PHLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,pH\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const PHLogEntry *ph = (const PHLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(ph->header.year, ph->header.month, ph->header.day, ph->header.hours, ph->header.minutes, ph->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f\r\n",
				d1,
				ph->ph);
		return std::string(entry);
	}
};


class PHSensorService : public SensorService {
public:
	PHSensorService(Sensor& sensor, Logger *logger) : SensorService(sensor, ServiceIdentifier::PH_SENSOR, "PH", logger) {}

private:

	void read_and_populate_log_entry(LogEntry *e) override {
		PHLogEntry *ph = (PHLogEntry *)e;
		ph->ph = m_sensor.read();
		service_set_log_header_time(ph->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = service_read_param<bool>(ParamID::PH_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::PH_SENSOR_PERIODIC);
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
