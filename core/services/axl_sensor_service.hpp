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
			bool   wakeup_triggered;
		};
		uint8_t data[MAX_LOG_PAYLOAD];
	};
};

class AXLLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,x,y,z,wakeup_triggered,temperature\r\n";
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
		snprintf(entry, sizeof(entry), "%s,%f,%f,%f,%u,%f\r\n",
				d1,
				log->x, log->y, log->z, log->wakeup_triggered, log->temperature);
		return std::string(entry);
	}
};

enum AXLSensorPort : unsigned int {
	TEMPERATURE,
	X,
	Y,
	Z,
	WAKEUP_TRIGGERED
};

enum AXLCalibration : unsigned int {
	WAKEUP_THRESH,
	WAKEUP_DURATION
};

enum AXLEvent : unsigned int {
	WAKEUP
};

class AXLSensorService : public SensorService {
public:
	AXLSensorService(Sensor& sensor, Logger *logger = nullptr) : SensorService(sensor, ServiceIdentifier::AXL_SENSOR, "AXL", logger) {}

private:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	void sensor_populate_log_entry(LogEntry *e, ServiceSensorData& data) override {
		AXLLogEntry *log = (AXLLogEntry *)e;
		log->x = data.port[(unsigned int)AXLSensorPort::X];
		log->y = data.port[(unsigned int)AXLSensorPort::Y];
		log->z = data.port[(unsigned int)AXLSensorPort::Z];
		log->wakeup_triggered = data.port[(unsigned int)AXLSensorPort::WAKEUP_TRIGGERED];
		log->temperature = data.port[(unsigned int)AXLSensorPort::TEMPERATURE];
		service_set_log_header_time(log->header, service_current_time());
	}
#pragma GCC diagnostic pop

	void sensor_init() override {
		// Setup 0.1G threshold and 1 sample duration
		double g_thresh = service_read_param<double>(ParamID::AXL_SENSOR_WAKEUP_THRESH);
		unsigned int duration = service_read_param<unsigned int>(ParamID::AXL_SENSOR_WAKEUP_SAMPLES);
		if (g_thresh && sensor_is_enabled()) {
			m_sensor.calibration_write(g_thresh, AXLCalibration::WAKEUP_THRESH);
			m_sensor.calibration_write(duration, AXLCalibration::WAKEUP_DURATION);
			m_sensor.install_event_handler(AXLEvent::WAKEUP, [this]() {
				DEBUG_TRACE("AXLSensorService::event");
				sensor_handler(false);
			});
		}
	};
	void sensor_term() override {
		m_sensor.remove_event_handler(AXLEvent::WAKEUP);
	};
	bool sensor_is_enabled() override {
		return service_read_param<bool>(ParamID::AXL_SENSOR_ENABLE);
	}
	unsigned int sensor_num_channels() override { return 5U; }
	unsigned int sensor_periodic() override {
		unsigned int schedule =
				1000 * service_read_param<unsigned int>(ParamID::AXL_SENSOR_PERIODIC);
		return schedule == 0 ? Service::SCHEDULE_DISABLED : schedule;
	}
};
