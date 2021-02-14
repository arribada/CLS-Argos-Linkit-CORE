#ifndef __GPS_SCHEDULER_HPP_
#define __GPS_SCHEDULER_HPP_

#include <functional>
#include <atomic>
#include "config_store.hpp"
#include "location_scheduler.hpp"
#include "scheduler.hpp"

class GPSScheduler : public LocationScheduler {
public:
	virtual ~GPSScheduler() {}
	void start(std::function<void()> data_notification_callback = nullptr) override;
	void stop() override;
	void notify_saltwater_switch_state(bool state) override;

protected:
	struct GNSSData {
		uint8_t  day;
		uint8_t  month;
		uint16_t year;
		uint8_t  hours;
		uint8_t  minutes;
		uint8_t  seconds;
		double   latitude;
		double   longitude;
		double   hDOP; // Horizontal Dilution of precision
	};

private:
	GNSSConfig m_gnss_config;

	struct {
		GNSSData data;
		std::atomic<bool> pending_rtc_set;
		std::atomic<bool> pending_data_logging;
	} m_gnss_data;

	std::function<void()> m_data_notification_callback;

	// Tasks
	Scheduler::TaskHandle m_task_acquisition_period;
	Scheduler::TaskHandle m_task_acquisition_timeout;
	Scheduler::TaskHandle m_task_update_rtc;
	Scheduler::TaskHandle m_task_process_gnss_data;
	void task_acquisition_period();
	void task_acquisition_timeout();
	void task_update_rtc();
	void task_process_gnss_data();

	void reschedule();
	void deschedule();
	uint32_t acquisition_period_to_seconds(BaseAqPeriod period);
	void populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time);
	void log_invalid_gps_entry();

	void gnss_data_callback(GNSSData data);
	void populate_gnss_data_and_callback();
	
	// These methods are specific to the chipset and should be implemented by device-specific subclass
	virtual void power_off() = 0;
	virtual void power_on(std::function<void(GNSSData data)> data_notification_callback = nullptr) = 0;
};

#endif // __GPS_SCHEDULER_HPP_
