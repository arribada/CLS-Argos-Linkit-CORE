#pragma once

#include "artic_device.hpp"
#include "service.hpp"

class ArgosTxService : public Service {
public:
	ArgosTxService(ArticDevice& device);

protected:
	// Service interface methods
	void service_init() override;
	void service_term() override;
	bool service_is_enabled() override;
	unsigned int service_next_schedule_in_ms() override;
	void service_initiate() override;
	bool service_cancel() override;
	unsigned int service_next_timeout() override;
	bool service_is_triggered_on_surfaced() override;
	bool service_is_usable_underwater() override;
	bool service_is_triggered_on_event(ServiceEvent&) override;

private:
	ArticDevice& m_artic;

	void react(ArticEventPowerOn&) {}
	void react(ArticEventPowerOff&) {}
	void react(ArticEventTxStarted&);
	void react(ArticEventTxComplete&);
	void react(ArticEventRxStarted&) {}
	void react(ArticEventRxPacket&) {}
	void react(ArticEventDeviceReady&) {}
	void react(ArticEventDeviceIdle&) {}
	void react(ArticEventDeviceError&);
};
