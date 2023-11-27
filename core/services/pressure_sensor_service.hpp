#pragma once

#include "logger.hpp"
#include "messages.hpp"
#include "sensor_service.hpp"
#include "timeutils.hpp"
#include "error.hpp"


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
	PressureSensorService(Sensor& sensor, Logger *logger = nullptr) : SensorService(sensor, ServiceIdentifier::PRESSURE_SENSOR, "PRESSURE", logger), m_last_pressure(0) {}

private:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	double m_last_pressure;

#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	void sensor_populate_log_entry(LogEntry *e, ServiceSensorData& data) override {
		PressureLogEntry *log = (PressureLogEntry *)e;
		log->pressure = data.port[(unsigned int)PressureSensorPort::PRESSURE];
		log->temperature = data.port[(unsigned int)PressureSensorPort::TEMPERATURE];

		// Check pressure logging mode
		BasePressureSensorLoggingMode mode = service_read_param<BasePressureSensorLoggingMode>(ParamID::PRESSURE_SENSOR_LOGGING_MODE);
		if (mode == BasePressureSensorLoggingMode::ALWAYS) {
			service_set_log_header_time(log->header, service_current_time());
		} else if (mode == BasePressureSensorLoggingMode::UW_THRESHOLD) {
			DEBUG_TRACE("PressureSensorService: using UW_THRESHOLD mode");
			double uw_threshold = service_read_param<double>(ParamID::UNDERWATER_DETECT_THRESH);
			if ((m_last_pressure < uw_threshold && log->pressure >= uw_threshold) ||
				(m_last_pressure >= uw_threshold && log->pressure < uw_threshold)) {
				// Trigger criteria of submerged or surfaced is met
				DEBUG_TRACE("PressureSensorService: threshold met (%f,%f)", m_last_pressure, log->pressure);
				m_last_pressure = log->pressure;
				service_set_log_header_time(log->header, service_current_time());
			} else {
				// Don't log if trigger criteria is not met
				DEBUG_TRACE("PressureSensorService: discarding sample (%f,%f)", m_last_pressure, log->pressure);
				m_last_pressure = log->pressure;
				throw ErrorCode::RESOURCE_NOT_AVAILABLE;
			}
		}
	}
#pragma GCC diagnostic pop

	unsigned int sensor_max_samples() override {
		return service_read_param<unsigned int>(ParamID::PRESSURE_SENSOR_ENABLE_TX_MAX_SAMPLES);
	}

	unsigned int sensor_num_channels() override { return 2U; }

	bool sensor_is_enabled() override {
		return service_read_param<bool>(ParamID::PRESSURE_SENSOR_ENABLE);
	}

	unsigned int sensor_periodic() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::PRESSURE_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}

	unsigned int sensor_tx_periodic() override {
		return service_read_param<unsigned int>(ParamID::PRESSURE_SENSOR_ENABLE_TX_SAMPLE_PERIOD);
	}

	bool sensor_is_usable_underwater() override { return true; }

	BaseSensorEnableTxMode sensor_enable_tx_mode() override {
		return service_read_param<BaseSensorEnableTxMode>(ParamID::PRESSURE_SENSOR_ENABLE_TX_MODE);
	}
};
