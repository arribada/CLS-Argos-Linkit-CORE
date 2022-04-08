#pragma once

#include "service.hpp"
#include "messages.hpp"

class Sensor : public Service {
public:
	Sensor(ServiceIdentifier service, char *name, Logger *logger) : Service(service, name, logger) {}
	virtual ~Sensor() {}
	virtual void calibrate(double value, unsigned int offset) = 0;
	virtual double read(unsigned int port = 0) = 0;
	virtual void read_and_populate_log_entry(LogEntry *e) = 0;

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
