#pragma once

#include "artic_device.hpp"
#include "service.hpp"

class ArgosRxService : public Service, ArticEventListener {
public:
	ArgosRxService(ArticDevice& device);
	void notify_peer_event(ServiceEvent& e);

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
	bool m_is_last_location_known;
	double m_last_longitude;
	double m_last_latitude;
	unsigned int m_next_timeout;
	std::map<uint8_t, AopSatelliteEntry_t> m_orbit_params_map;
	std::map<uint8_t, AopSatelliteEntry_t> m_constellation_status_map;

	void react(ArticEventRxPacket const&) override;
	void react(ArticEventDeviceError const&) override;

	void update_pass_predict(BasePassPredict& new_pass_predict);
};
