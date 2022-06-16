#pragma once

#include "artic_device.hpp"
#include "service.hpp"


class ArgosRxScheduler {
private:
	struct Location {
		double longitude;
		double latitude;
		Location(double x, double y) {
			longitude = x;
			latitude = y;
		}
	};
	std::time_t m_earliest_schedule;
	std::optional<Location> m_location;

	static inline const unsigned int SECONDS_PER_MINUTE = 60;
	static inline const unsigned int MINUTES_PER_HOUR   = 60;
	static inline const unsigned int HOURS_PER_DAY      = 24;
	static inline const unsigned int MSECS_PER_SECOND   = 1000;
	static inline const unsigned int SECONDS_PER_HOUR   = MINUTES_PER_HOUR * SECONDS_PER_MINUTE;
	static inline const unsigned int SECONDS_PER_DAY    = HOURS_PER_DAY * SECONDS_PER_HOUR;
	static inline const unsigned int ARGOS_RX_MARGIN_MSECS = 0;

public:
	static inline const unsigned int INVALID_SCHEDULE   = (unsigned int)-1;

	ArgosRxScheduler();
	unsigned int schedule(ArgosConfig& config, BasePassPredict& pass_predict, std::time_t now, unsigned int &timeout, ArticMode& scheduled_mode);
	void set_earliest_schedule(std::time_t t);
	void set_location(double lon, double lat);
};


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
	bool service_is_triggered_on_surfaced(bool&) override;

private:
	ArticDevice& m_artic;
	ArgosRxScheduler m_sched;
	unsigned int m_timeout;
	ArticMode    m_mode;
	std::map<uint8_t, AopSatelliteEntry_t> m_orbit_params_map;
	std::map<uint8_t, AopSatelliteEntry_t> m_constellation_status_map;
	unsigned int m_cumulative_rx_time;

	void react(ArticEventRxPacket const&) override;
	void react(ArticEventDeviceError const&) override;
	void react(ArticEventPowerOff const&) override;
	void react(ArticEventRxStopped const&) override;

	void update_pass_predict(BasePassPredict& new_pass_predict);
};
