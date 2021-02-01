#ifndef __GPS_SCHEDULER_HPP_
#define __GPS_SCHEDULER_HPP_

#include <functional>
#include "config_store.hpp"
#include "scheduler.hpp"

class GPSScheduler {
public:
	virtual ~GPSScheduler() {}
	void start(std::function<void()> data_notification_callback = nullptr);
	void stop();
	void notify_saltwater_switch_state(bool state);

protected:
	struct GNSSData {
		uint8_t  day;
		uint8_t  month;
		uint16_t year;
		uint8_t  hours;
		uint8_t  minutes;
		uint8_t  seconds;
		double latitude;
		double longitude;
		double hDOP; // HorizontaldDilution of precision
	};

private:
	GNSSConfig m_gnss_config;
	Scheduler::TaskHandle m_acquisition_period_task;
	Scheduler::TaskHandle m_acquisition_timeout_task;

	std::function<void()> m_data_notification_callback;

	void reschedule();
	void deschedule();
	uint32_t acquisition_period_to_seconds(BaseAqPeriod period);
	void populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time);

	void acquisition_period_task();
	void acquisition_timeout_task();
	void gnss_data_callback(GNSSData data);

	// These methods are specific to the chipset and should be implemented by device-specific subclass
	virtual void power_off() = 0;
	virtual void power_on(std::function<void(GNSSData data)> data_notification_callback = nullptr) = 0;
};

#endif // __GPS_SCHEDULER_HPP_
