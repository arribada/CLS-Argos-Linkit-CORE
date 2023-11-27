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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	void sensor_populate_log_entry(LogEntry *e, ServiceSensorData& data) override {
		ALSLogEntry *log = (ALSLogEntry *)e;
		log->lumens = data.port[0];
		service_set_log_header_time(log->header, service_current_time());
	}
#pragma GCC diagnostic pop

	unsigned int sensor_max_samples() override {
		return service_read_param<unsigned int>(ParamID::ALS_SENSOR_ENABLE_TX_MAX_SAMPLES);
	}

	unsigned int sensor_num_channels() override { return 1U; }

	bool sensor_is_enabled() override {
		return service_read_param<bool>(ParamID::ALS_SENSOR_ENABLE);
	}

	unsigned int sensor_periodic() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::ALS_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}

	unsigned int sensor_tx_periodic() override {
		return service_read_param<unsigned int>(ParamID::ALS_SENSOR_ENABLE_TX_SAMPLE_PERIOD);
	}

	BaseSensorEnableTxMode sensor_enable_tx_mode() override {
		return service_read_param<BaseSensorEnableTxMode>(ParamID::ALS_SENSOR_ENABLE_TX_MODE);
	}
};
