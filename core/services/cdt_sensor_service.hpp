#pragma once

#include "sensor_service.hpp"
#include "logger.hpp"
#include "messages.hpp"
#include "timeutils.hpp"


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


class CDTSensorService : public SensorService {
public:
	CDTSensorService(Sensor& sensor, Logger *logger) : SensorService(sensor, ServiceIdentifier::CDT_SENSOR, "CDT", logger) {}

private:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	void sensor_populate_log_entry(LogEntry *e, ServiceSensorData& data) override {
		CDTLogEntry *log = (CDTLogEntry *)e;
		log->conductivity = data.port[(unsigned int)CDTSensorPort::CONDUCTIVITY];
		log->depth = data.port[(unsigned int)CDTSensorPort::DEPTH];
		log->temperature = data.port[(unsigned int)CDTSensorPort::TEMPERATURE];
		service_set_log_header_time(log->header, service_current_time());
	}
#pragma GCC diagnostic pop

	unsigned int sensor_num_channels() override { return 3U; }

	bool sensor_is_enabled() override {
		return service_read_param<bool>(ParamID::CDT_SENSOR_ENABLE);
	}

	unsigned int sensor_periodic() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::CDT_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}
};
