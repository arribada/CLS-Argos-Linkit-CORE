#ifndef __GPS_SCHEDULER_HPP_
#define __GPS_SCHEDULER_HPP_

#include <functional>
#include <atomic>
#include "config_store.hpp"
#include "service_scheduler.hpp"
#include "scheduler.hpp"

struct GPSNavSettings {
	BaseGNSSFixMode  fix_mode;
	BaseGNSSDynModel dyn_model;
};

class GPSScheduler : public ServiceScheduler {
public:
	virtual ~GPSScheduler() {}
	void start(std::function<void(ServiceEvent)> data_notification_callback = nullptr) override;
	void stop() override;
	void notify_saltwater_switch_state(bool state) override;
	void notify_sensor_log_update() override {};

protected:
	struct GNSSData {
		uint32_t   iTOW;
		uint16_t   year;
		uint8_t    month;
		uint8_t    day;
		uint8_t    hour;
		uint8_t    min;
		uint8_t    sec;
		uint8_t    valid;
		uint32_t   tAcc;
		int32_t    nano;
		uint8_t    fixType;
		uint8_t    flags;
		uint8_t    flags2;
		uint8_t    flags3;
		uint8_t    numSV;
		double     lon;       // Degrees
		double     lat;       // Degrees
		int32_t    height;    // mm
		int32_t    hMSL;      // mm
		uint32_t   hAcc;      // mm
		uint32_t   vAcc;      // mm
		int32_t    velN;      // mm
		int32_t    velE;      // mm
		int32_t    velD;      // mm
		int32_t    gSpeed;    // mm/s
		float      headMot;   // Degrees
		uint32_t   sAcc;      // mm/s
		float      headAcc;   // Degrees
		float      pDOP;
		float      vDOP;
		float      hDOP;
		float      headVeh;   // Degrees
	};

private:
	GNSSConfig m_gnss_config;
	bool       m_is_first_fix_found;

	struct {
		GNSSData data;
		std::atomic<bool> pending_rtc_set;
		std::atomic<bool> pending_data_logging;
	} m_gnss_data;

	std::function<void(ServiceEvent)> m_data_notification_callback;

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
	void populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time);
	void log_invalid_gps_entry();

	void gnss_data_callback(GNSSData data);
	void populate_gnss_data_and_callback();
	
	// These methods are specific to the chipset and should be implemented by device-specific subclass
	virtual void power_off() = 0;
	virtual void power_on(const GPSNavSettings& nav_settings,
						  std::function<void(GNSSData data)> data_notification_callback = nullptr) = 0;
};

#endif // __GPS_SCHEDULER_HPP_
