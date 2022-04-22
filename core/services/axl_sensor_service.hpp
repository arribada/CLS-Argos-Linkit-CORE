#pragma once

#include "logger.hpp"
#include "messages.hpp"
#include "sensor_service.hpp"
#include "timeutils.hpp"

struct __attribute__((packed)) AXLLogEntry {
	LogHeader header;
	union {
		struct {
			double x;
			double y;
			double z;
			double temperature;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

class AXLLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,x,y,z,temperature\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const AXLLogEntry *log = (const AXLLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(log->header.year, log->header.month, log->header.day, log->header.hours, log->header.minutes, log->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f,%f,%f,%f\r\n",
				d1,
				log->x, log->y, log->z, log->temperature);
		return std::string(entry);
	}
};

enum class AXLSensorPort : unsigned int {
	TEMPERATURE,
	X,
	Y,
	Z,
};


class AXLSensorService : public SensorService {
public:
	AXLSensorService(Sensor& sensor, Logger *logger = nullptr) : SensorService(sensor, ServiceIdentifier::AXL_SENSOR, "AXL", logger) {}

private:
	void read_and_populate_log_entry(LogEntry *e) override {
		AXLLogEntry *log = (AXLLogEntry *)e;
		log->x = m_sensor.read((unsigned int)AXLSensorPort::X);
		log->y = m_sensor.read((unsigned int)AXLSensorPort::Y);
		log->z = m_sensor.read((unsigned int)AXLSensorPort::Z);
		log->temperature = m_sensor.read((unsigned int)AXLSensorPort::TEMPERATURE);
		service_set_log_header_time(log->header, service_current_time());
	}

	void service_init() override {};
	void service_term() override {};
	bool service_is_enabled() override {
		bool enable = service_read_param<bool>(ParamID::AXL_SENSOR_ENABLE);
		return enable;
	}
	unsigned int service_next_schedule_in_ms() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::AXL_SENSOR_PERIODIC);
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
