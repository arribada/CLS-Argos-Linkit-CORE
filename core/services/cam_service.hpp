#pragma once

#include <atomic>

#include "runcam.hpp"
#include "service.hpp"
#include "logger.hpp"
#include "timeutils.hpp"
#include "scheduler.hpp"


class CAMLogFormatter : public LogFormatter {
public:
	const std::string header() override {
		return "log_datetime,id,batt_voltage,state\r\n";
	}
	const std::string log_entry(const LogEntry& e) override {
		char entry[512], d1[128];
		const CAMLogEntry *cam = (const CAMLogEntry *)&e;
		std::time_t t;
		std::tm *tm;

		t = convert_epochtime(cam->header.year, cam->header.month, cam->header.day, cam->header.hours, cam->header.minutes, cam->header.seconds);
		tm = std::gmtime(&t);
		std::strftime(d1, sizeof(d1), "%d/%m/%Y %H:%M:%S", tm);

		// Convert to CSV
		snprintf(entry, sizeof(entry), "%s,%d,%f,%d\r\n",
				d1,
				(unsigned int)cam->info.counter,
				(double)cam->info.batt_voltage/1000,
				(unsigned int)cam->info.event_type);
		return std::string(entry);
	};
};

class CAMService : public Service, public CAMEventListener  {
public:
	CAMService(CAMDevice& device, Logger *logger) : Service(ServiceIdentifier::CAM_SENSOR, "CAM", logger), m_device(device) {
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
	CAMDevice&   m_device;
	bool         m_is_underwater;
	uint64_t     m_wakeup_time;
	std::time_t  m_next_schedule;
	bool m_is_active;
	unsigned int m_num_captures;

    void react(const CAMEventError&) override;

	// Private methods for GNSS
	void task_process_cam_data();
	void populate_cam_log_with_time(CAMLogEntry &entry, std::time_t time);
	CAMLogEntry invalid_log_entry();
	void cam_data_callback();
	void populate_cam_data_and_callback();
};
