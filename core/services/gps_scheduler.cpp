#include "gps_scheduler.hpp"
#include "config_store.hpp"
#include "rtc.hpp"
#include "battery.hpp"
#include "timer.hpp"


extern ConfigurationStore *configuration_store;
extern BatteryMonitor *battery_monitor;
extern RTC *rtc;
extern Timer *system_timer;


#define MS_PER_SEC         (1000)
#define FIRST_AQPERIOD_SEC (30)     // Schedule for first AQPERIOD to accelerate first fix


void GPSScheduler::service_init() {
	m_is_active = false;
    m_gnss_data.pending_data_logging = false;
    m_gnss_data.pending_rtc_set = false;
    m_is_first_fix_found = false;
    m_is_first_schedule = true;
    m_num_gps_fixes = 0;
	power_off();
}

void GPSScheduler::service_term() {
}

bool GPSScheduler::service_is_enabled() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	return gnss_config.enable;
}

unsigned int GPSScheduler::service_next_schedule_in_ms() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);

    std::time_t now = rtc->gettime();
    std::time_t aq_period = m_is_first_schedule ? FIRST_AQPERIOD_SEC : (m_is_first_fix_found ? gnss_config.dloc_arg_nom : gnss_config.cold_start_retry_period);

    if (aq_period == 0) {
    	return Service::SCHEDULE_DISABLED;
    }

    DEBUG_TRACE("GPSScheduler::reschedule: is_first=%u first_fix=%u cold=%u aqperiod=%u",
    		(unsigned int)m_is_first_schedule, (unsigned int)m_is_first_fix_found, (unsigned int)m_gnss_config.cold_start_retry_period, (unsigned int)aq_period);

    // Find the next schedule time aligned to UTC 00:00
    std::time_t next_schedule = now - (now % aq_period) + aq_period;

    // Find the time in milliseconds until this schedule
    return (next_schedule - now) * MS_PER_SEC;
}

bool GPSScheduler::service_initiate() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	GPSNavSettings nav_settings = {
		gnss_config.fix_mode,
		gnss_config.dyn_model,
		gnss_config.assistnow_enable
	};

	m_next_schedule = rtc->gettime();
	m_is_first_schedule = false;
	m_wakeup_time = system_timer->get_counter();
	m_num_consecutive_fixes = gnss_config.min_num_fixes;

	try {
		power_on(nav_settings,
			[this](GNSSData data) {
				gnss_data_callback(data);
			}
		);
	} catch (...) {
		ServiceEventData event_data = false;
		service_complete(event_data, invalid_log_entry());
	}
}

bool GPSScheduler::service_cancel(bool is_timeout) {
	// Cleanly terminate
	system_scheduler->cancel_task(m_task_update_rtc);
	system_scheduler->cancel_task(m_task_process_gnss_data);
	power_off();

	if (is_timeout && m_is_active) {
		ServiceEventData event_data = false;
		service_complete(event_data, invalid_log_entry());
	}

	m_is_active = false;
}

unsigned int GPSScheduler::service_next_timeout() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	return m_is_first_fix_found ? gnss_config.acquisition_timeout_cold_start : gnss_config.acquisition_timeout;
}

bool GPSScheduler::service_is_triggered_on_surfaced() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	return gnss_config.trigger_on_surfaced;
}

bool GPSScheduler::service_is_usable_underwater() {
	return false;
}



GPSLogEntry GPSScheduler::invalid_log_entry()
{
    DEBUG_INFO("GPSScheduler::invalid_log_entry");

    GPSLogEntry gps_entry;
    memset(&gps_entry, 0, sizeof(gps_entry));

    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, rtc->gettime());

    gps_entry.info.batt_voltage = battery_monitor->get_voltage();
    gps_entry.info.event_type = GPSEventType::NO_FIX;
    gps_entry.info.valid = false;
    gps_entry.info.onTime = system_timer->get_counter() - m_wakeup_time;
    gps_entry.info.schedTime = m_next_schedule;

    return gps_entry;
}

void GPSScheduler::task_update_rtc()
{
    rtc->settime(convert_epochtime(m_gnss_data.data.year, m_gnss_data.data.month, m_gnss_data.data.day, m_gnss_data.data.hour, m_gnss_data.data.min, m_gnss_data.data.sec));
    DEBUG_TRACE("GPSScheduler::task_update_rtc");
    m_gnss_data.pending_rtc_set = false;
}

void GPSScheduler::task_process_gnss_data()
{
    DEBUG_TRACE("GPSScheduler::task_process_gnss_data");

    GPSLogEntry gps_entry;
    memset(&gps_entry, 0, sizeof(gps_entry));

    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, rtc->gettime());

    gps_entry.info.batt_voltage = battery_monitor->get_voltage();

    // Store GPS data
    gps_entry.info.iTOW          = m_gnss_data.data.iTOW;
    gps_entry.info.year          = m_gnss_data.data.year;
    gps_entry.info.month         = m_gnss_data.data.month;
    gps_entry.info.day           = m_gnss_data.data.day;
    gps_entry.info.hour          = m_gnss_data.data.hour;
    gps_entry.info.min           = m_gnss_data.data.min;
    gps_entry.info.sec           = m_gnss_data.data.sec;
    gps_entry.info.valid         = m_gnss_data.data.valid;
    gps_entry.info.fixType       = m_gnss_data.data.fixType;
    gps_entry.info.flags         = m_gnss_data.data.flags;
    gps_entry.info.flags2        = m_gnss_data.data.flags2;
    gps_entry.info.flags3        = m_gnss_data.data.flags3;
    gps_entry.info.numSV         = m_gnss_data.data.numSV;
    gps_entry.info.lon           = m_gnss_data.data.lon;
    gps_entry.info.lat           = m_gnss_data.data.lat;
    gps_entry.info.height        = m_gnss_data.data.height;
    gps_entry.info.hMSL          = m_gnss_data.data.hMSL;
    gps_entry.info.hAcc          = m_gnss_data.data.hAcc;
    gps_entry.info.vAcc          = m_gnss_data.data.vAcc;
    gps_entry.info.velN          = m_gnss_data.data.velN;
    gps_entry.info.velE          = m_gnss_data.data.velE;
    gps_entry.info.velD          = m_gnss_data.data.velD;
    gps_entry.info.gSpeed        = m_gnss_data.data.gSpeed;
    gps_entry.info.headMot       = m_gnss_data.data.headMot;
    gps_entry.info.sAcc          = m_gnss_data.data.sAcc;
    gps_entry.info.headAcc       = m_gnss_data.data.headAcc;
    gps_entry.info.pDOP          = m_gnss_data.data.pDOP;
    gps_entry.info.vDOP          = m_gnss_data.data.vDOP;
    gps_entry.info.hDOP          = m_gnss_data.data.hDOP;
    gps_entry.info.headVeh       = m_gnss_data.data.headVeh;
    gps_entry.info.ttff          = m_gnss_data.data.ttff;
    gps_entry.info.onTime        = system_timer->get_counter() - m_wakeup_time;

    if (m_num_gps_fixes == 1) {
    	// For the very first fix, we need to compute the scheduled GPS time since the RTC
    	// wasn't set beforehand
    	std::time_t fix_time = convert_epochtime(gps_entry.info.year, gps_entry.info.month, gps_entry.info.day, gps_entry.info.hour, gps_entry.info.min, gps_entry.info.sec);
    	gps_entry.info.schedTime     = fix_time - (gps_entry.info.onTime / MS_PER_SEC);
    } else {
    	gps_entry.info.schedTime     = m_next_schedule;
    }

    gps_entry.info.event_type = GPSEventType::FIX;
    gps_entry.info.valid = true;

    DEBUG_INFO("GPSScheduler::task_process_gnss_data: lat=%lf lon=%lf hDOP=%lf hAcc=%lf numSV=%u batt=%lfV ", gps_entry.info.lat, gps_entry.info.lon,
    		static_cast<double>(gps_entry.info.hDOP),
			static_cast<double>(gps_entry.info.hAcc),
			gps_entry.info.numSV,
			(double)gps_entry.info.batt_voltage / 1000);

    m_is_active = false;
    power_off();
    m_gnss_data.pending_data_logging = false;

    // Notify configuration store that we have a new valid GPS fix
    configuration_store->notify_gps_location(gps_entry);

    ServiceEventData event_data = true;
    service_complete(data, &gps_entry);
}

void GPSScheduler::gnss_data_callback(GNSSData data) {
    // If we haven't finished processing our last data then ignore this one
    if (m_gnss_data.pending_data_logging || m_gnss_data.pending_rtc_set)
        return;

    m_gnss_data.data = data;

    // Update our time based off this data, schedule this as high priority
    m_gnss_data.pending_rtc_set = true;
    m_task_update_rtc = system_scheduler->post_task_prio([this]() {
    	task_update_rtc();
    }, "GPSSchedulerUpdateRTC", Scheduler::HIGHEST_PRIORITY);

    // Only process this data if it satisfies an optional hdop threshold
    if (m_gnss_config.hdop_filter_enable && (m_gnss_data.data.hDOP > m_gnss_config.hdop_filter_threshold)) {
    	m_num_consecutive_fixes = m_gnss_config.min_num_fixes;
    	DEBUG_TRACE("GPSScheduler::gnss_data_callback: HDOP threshold %u not met with %f", m_gnss_config.hdop_filter_threshold, (double)m_gnss_data.data.hDOP);
    	return;
    }

    // Only process this data if it satisfies an optional hacc threshold
    if (m_gnss_config.hacc_filter_enable && (m_gnss_data.data.hAcc > 1000 * m_gnss_config.hacc_filter_threshold)) {
    	m_num_consecutive_fixes = m_gnss_config.min_num_fixes;
    	DEBUG_TRACE("GPSScheduler::gnss_data_callback: HACC threshold %u not met with %f", 1000 * m_gnss_config.hacc_filter_threshold, (double)m_gnss_data.data.hAcc);
    	return;
    }

    // Now check the requisite number of consecutive fixes have been made
    if (--m_num_consecutive_fixes) {
       	DEBUG_TRACE("GPSScheduler::gnss_data_callback: criteria met with %u consecutive fixes remaining", m_num_consecutive_fixes);
        return;
    }

    // Mark first fix flag
    m_is_first_fix_found = true;
    m_num_gps_fixes++;

    // All filter criteria is met
    {
        // Defer processing this data till we are outside of this interrupt context
        m_gnss_data.pending_data_logging = true;
        m_task_process_gnss_data = system_scheduler->post_task_prio(
        		[this]() { task_process_gnss_data(); },
        		"GPSSchedulerProcessGNSSData",
        		Scheduler::DEFAULT_PRIORITY);
    }
}

void GPSScheduler::populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time)
{
	service_set_log_header_time(entry.header, time);
}