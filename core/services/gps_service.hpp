#pragma once

#include <atomic>

#include "gps.hpp"
#include "service.hpp"
#include "logger.hpp"
#include "timeutils.hpp"
#include "scheduler.hpp"


class GPSLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,batt_voltage,iTOW,fix_datetime,valid,onTime,ttff,fixType,flags,flags2,flags3,numSV,lon,lat,height,hMSL,hAcc,vAcc,velN,velE,velD,gSpeed,headMot,sAcc,headAcc,pDOP,vDOP,hDOP,headVeh\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128], d2[128];
		const GPSLogEntry *gps = (const GPSLogEntry *)&e;

		snprintf(d1, sizeof(d1), "%02hhu/%02hhu/%04hu %02hhu:%02hhu:%02hhu",
		        gps->header.day, gps->header.month, gps->header.year,
		        gps->header.hours, gps->header.minutes, gps->header.seconds);
        snprintf(d2, sizeof(d2), "%02hhu/%02hhu/%04hu %02hhu:%02hhu:%02hhu",
                gps->info.day, gps->info.month, gps->info.year,
                gps->info.hour, gps->info.min, gps->info.sec);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%f,%u,%s,%u,%u,%u,%u,%u,%u,%u,%u,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f\r\n",
				d1,
				(double)gps->info.batt_voltage/1000,
				(unsigned int)gps->info.iTOW,
				d2,
				(unsigned int)gps->info.valid,
				(unsigned int)gps->info.onTime,
				(unsigned int)gps->info.ttff,
				(unsigned int)gps->info.fixType,
				(unsigned int)gps->info.flags,
				(unsigned int)gps->info.flags2,
				(unsigned int)gps->info.flags3,
				(unsigned int)gps->info.numSV,
				gps->info.lon,
				gps->info.lat,
				(double)gps->info.height / 1000,
				(double)gps->info.hMSL / 1000,
				(double)gps->info.hAcc / 1000,
				(double)gps->info.vAcc / 1000,
				(double)gps->info.velN / 1000,
				(double)gps->info.velE / 1000,
				(double)gps->info.velD / 1000,
				(double)gps->info.gSpeed / 1000,
				(double)gps->info.headMot,
				(double)gps->info.sAcc / 1000,
				(double)gps->info.headAcc,
				(double)gps->info.pDOP,
				(double)gps->info.vDOP,
				(double)gps->info.hDOP,
				(double)gps->info.headVeh);
		return std::string(entry);
	}
};

class GPSService : public Service, public GPSEventListener  {
public:
	GPSService(GPSDevice& device, Logger *logger) : Service(ServiceIdentifier::GNSS_SENSOR, "GNSS", logger), m_device(device) {
	    m_device.subscribe(*this);
	}
	void notify_peer_event(ServiceEvent& e) override;

protected:

	// Service interface methods
	void service_init() override;
	void service_term() override;
	bool service_is_enabled() override;
	unsigned int service_next_schedule_in_ms() override;
	void service_initiate() override;
	bool service_cancel() override;
	unsigned int service_next_timeout() override;
	bool service_is_triggered_on_surfaced(bool &) override;
	bool service_is_usable_underwater() override;
	bool service_is_triggered_on_event(ServiceEvent&, bool&) override;

private:
	GPSDevice&   m_device;
	bool       	 m_is_first_fix_found;
	bool       	 m_is_first_schedule;
	bool         m_is_underwater;
	uint64_t     m_wakeup_time;
	std::time_t  m_next_schedule;
	struct {
		GNSSData data;
	} m_gnss_data;
	unsigned int m_num_gps_fixes;
	bool m_is_active;

    void react(const GPSEventMaxNavSamples&) override;
    void react(const GPSEventPVT&) override;
    void react(const GPSEventError&) override;

	// Private methods for GNSS
	void task_process_gnss_data();
	void populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time);
	GPSLogEntry invalid_log_entry();
	void gnss_data_callback(GNSSData data);
	void populate_gnss_data_and_callback();
};
