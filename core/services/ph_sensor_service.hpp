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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	void sensor_populate_log_entry(LogEntry *e, ServiceSensorData& data) override {
		PHLogEntry *ph = (PHLogEntry *)e;
		ph->ph = data.port[0];
		service_set_log_header_time(ph->header, service_current_time());
	}
#pragma GCC diagnostic pop

	unsigned int sensor_max_samples() override {
		return service_read_param<unsigned int>(ParamID::PH_SENSOR_ENABLE_TX_MAX_SAMPLES);
	}

	unsigned int sensor_num_channels() override { return 1U; }

	bool sensor_is_enabled() override {
		return service_read_param<bool>(ParamID::PH_SENSOR_ENABLE);
	}

	unsigned int sensor_periodic() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::PH_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}

	unsigned int sensor_tx_periodic() override {
		return service_read_param<unsigned int>(ParamID::PH_SENSOR_ENABLE_TX_SAMPLE_PERIOD);
	}

	bool sensor_is_usable_underwater() override { return true; }

	BaseSensorEnableTxMode sensor_enable_tx_mode() override {
		return service_read_param<BaseSensorEnableTxMode>(ParamID::PH_SENSOR_ENABLE_TX_MODE);
	}
};
