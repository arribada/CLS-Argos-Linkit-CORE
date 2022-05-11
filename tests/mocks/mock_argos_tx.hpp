#pragma once

#include "mock_comparators.hpp"
#include "service.hpp"

class MockArgosTxService : public Service {
public:
	MockArgosTxService() : Service(ServiceIdentifier::ARGOS_TX, "ARGOSTX") {}


private:
	void service_init() override {}
	void service_term() override {}
	bool service_is_enabled() override { return false; }
	unsigned int service_next_schedule_in_ms() override { return Service::SCHEDULE_DISABLED; }
	void service_initiate() override {}
};
