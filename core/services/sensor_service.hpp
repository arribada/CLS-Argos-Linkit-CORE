#pragma once

#include "sensor.hpp"
#include "service.hpp"
#include "logger.hpp"

class SensorService : public Service {
public:
	SensorService(Sensor& sensor, ServiceIdentifier service, const char *name, Logger *logger) : Service(service, name, logger), m_sensor(sensor) {}
	virtual ~SensorService() {}
	virtual void read_and_populate_log_entry(LogEntry *e) = 0;

protected:
	Sensor &m_sensor;

private:
	virtual void service_init() = 0;
	virtual void service_term() = 0;
	virtual bool service_is_enabled() = 0;
	virtual unsigned int service_next_schedule_in_ms() = 0;
	virtual void service_initiate() = 0;
	virtual bool service_cancel() = 0;
	virtual unsigned int service_next_timeout() = 0;
	virtual bool service_is_triggered_on_surfaced() = 0;
	virtual bool service_is_usable_underwater() = 0;
};
