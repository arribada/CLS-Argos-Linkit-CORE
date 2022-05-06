#pragma once

#include "mock_comparators.hpp"
#include "service.hpp"

class MockLocationScheduler : public Service {
public:
	MockLocationScheduler(Logger *logger = nullptr) : Service(ServiceIdentifier::GNSS_SENSOR, "GNSS", logger) {}

private:
	void service_init() override {}
	void service_term() override {}
	bool service_is_enabled() override { return false; }
	unsigned int service_next_schedule_in_ms() override { return Service::SCHEDULE_DISABLED; }
	void service_initiate() override {}
};
