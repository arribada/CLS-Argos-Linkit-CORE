#pragma once

#include "sensor_service.hpp"
#include "logger.hpp"
#include "messages.hpp"
#include "timeutils.hpp"


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
				log->sea_temp);
		return std::string(entry);
	}
};


class SeaTempSensorService : public SensorService {
public:
	SeaTempSensorService(Sensor& sensor, Logger *logger) : SensorService(sensor, ServiceIdentifier::SEA_TEMP_SENSOR, "SEA_TEMP", logger) {}

private:

	void read_and_populate_log_entry(LogEntry *e) override {
		SeaTempLogEntry *log = (SeaTempLogEntry *)e;
		log->sea_temp = m_sensor.read();
		service_set_log_header_time(log->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = service_read_param<bool>(ParamID::SEA_TEMP_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::SEA_TEMP_SENSOR_PERIODIC);
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
