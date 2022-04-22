#pragma once

#include "logger.hpp"
#include "messages.hpp"
#include "sensor_service.hpp"
#include "timeutils.hpp"

struct __attribute__((packed)) PressureLogEntry {
	LogHeader header;
	union {
		struct {
			double pressure;
			double temperature;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

class PressureLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,pressure,temperature\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const PressureLogEntry *log = (const PressureLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(log->header.year, log->header.month, log->header.day, log->header.hours, log->header.minutes, log->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f,%f\r\n",
				d1,
				log->pressure, log->temperature);
		return std::string(entry);
	}
};

enum class PressureSensorPort : unsigned int {
	PRESSURE,
	TEMPERATURE,
};


class PressureSensorService : public SensorService {
public:
	PressureSensorService(Sensor& sensor, Logger *logger = nullptr) : SensorService(sensor, ServiceIdentifier::PRESSURE_SENSOR, "PRESSURE", logger) {}

private:
	void read_and_populate_log_entry(LogEntry *e) override {
		PressureLogEntry *log = (PressureLogEntry *)e;
		log->pressure = m_sensor.read((unsigned int)PressureSensorPort::PRESSURE);
		log->temperature = m_sensor.read((unsigned int)PressureSensorPort::TEMPERATURE);
		service_set_log_header_time(log->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = service_read_param<bool>(ParamID::PRESSURE_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::PRESSURE_SENSOR_PERIODIC);
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
