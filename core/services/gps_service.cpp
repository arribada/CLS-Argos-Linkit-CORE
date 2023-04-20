#include "gps_service.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;


#define MS_PER_SEC         (1000)
#define FIRST_AQPERIOD_SEC (30)     // Schedule for first AQPERIOD to accelerate first fix


void GPSService::service_init() {
	m_is_active = false;
    m_gnss_data.pending_data_logging = false;
    m_is_first_fix_found = false;
    m_is_first_schedule = true;
    m_num_gps_fixes = 0;
}

void GPSService::service_term() {
}

bool GPSService::service_is_enabled() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	return gnss_config.enable;
}

unsigned int GPSService::service_next_schedule_in_ms() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);

    std::time_t now = service_current_time();
    std::time_t aq_period = m_is_first_schedule ? FIRST_AQPERIOD_SEC : (m_is_first_fix_found ? gnss_config.dloc_arg_nom : gnss_config.cold_start_retry_period);

    if (aq_period == 0) {
    	return Service::SCHEDULE_DISABLED;
    }

    // Find the next schedule time aligned to UTC 00:00
    std::time_t next_schedule = now - (now % aq_period) + aq_period;

    DEBUG_TRACE("GPSService::reschedule: is_first=%u first_fix=%u cold=%u aqperiod=%u now=%u next=%u",
    		(unsigned int)m_is_first_schedule, (unsigned int)m_is_first_fix_found, (unsigned int)gnss_config.cold_start_retry_period, (unsigned int)aq_period,
			(unsigned int)now, (unsigned int)next_schedule);

    // Find the time in milliseconds until this schedule
    return (next_schedule - now) * MS_PER_SEC;
}

void GPSService::service_initiate() {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	GPSNavSettings nav_settings = {
		gnss_config.fix_mode,
		gnss_config.dyn_model,
		gnss_config.assistnow_enable,
		gnss_config.assistnow_offline_enable,
		gnss_config.hdop_filter_enable,
		gnss_config.hdop_filter_threshold,
		gnss_config.hacc_filter_enable,
		gnss_config.hacc_filter_threshold,
	};

	nav_settings.num_consecutive_fixes = gnss_config.min_num_fixes;
	nav_settings.sat_tracking = true;
	nav_settings.acquisition_timeout = MS_PER_SEC * (m_is_first_fix_found ? gnss_config.acquisition_timeout : gnss_config.acquisition_timeout_cold_start);

	m_next_schedule = service_current_time();
	m_is_first_schedule = false;
	m_wakeup_time = service_current_timer();

	try {
		m_is_active = true;
		m_device.power_on(nav_settings);
	} catch (...) {
		m_is_active = false;
		GPSLogEntry log_entry = invalid_log_entry();
		ServiceEventData event_data = log_entry;
		service_complete(&event_data, &log_entry);
	}
}

bool GPSService::service_cancel() {
	// Cleanly terminate
	DEBUG_TRACE("GPSService::service_cancel");

	if (m_is_active) {
		m_is_active = false;
		m_device.power_off();
		system_scheduler->cancel_task(m_task_process_gnss_data);
		GPSLogEntry log_entry = invalid_log_entry();
		ServiceEventData event_data = log_entry;
		service_complete(&event_data, &log_entry);
		return true;
	}

	return false;
}

unsigned int GPSService::service_next_timeout() {
	// Timeout are handled in the GPSDevice
	return 0;
}

bool GPSService::service_is_triggered_on_surfaced(bool& immediate) {
	GNSSConfig gnss_config;
	configuration_store->get_gnss_configuration(gnss_config);
	immediate = gnss_config.trigger_on_surfaced;
	return true;
}

bool GPSService::service_is_usable_underwater() {
	return false;
}

GPSLogEntry GPSService::invalid_log_entry()
{
    DEBUG_INFO("GPSService::invalid_log_entry");

    GPSLogEntry gps_entry;
    memset(&gps_entry, 0, sizeof(gps_entry));

    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, service_current_time());

    gps_entry.info.batt_voltage = service_get_voltage();
    gps_entry.info.event_type = GPSEventType::NO_FIX;
    gps_entry.info.valid = false;
    gps_entry.info.onTime = service_current_timer() - m_wakeup_time;
    gps_entry.info.schedTime = m_next_schedule;

    return gps_entry;
}


void GPSService::task_process_gnss_data()
{
    DEBUG_TRACE("GPSService::task_process_gnss_data");

    GPSLogEntry gps_entry;
    memset(&gps_entry, 0, sizeof(gps_entry));

    gps_entry.header.log_type = LOG_GPS;

    populate_gps_log_with_time(gps_entry, service_current_time());

    gps_entry.info.batt_voltage = service_get_voltage();

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
    gps_entry.info.onTime        = service_current_timer() - m_wakeup_time;

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

    DEBUG_INFO("GPSService::task_process_gnss_data: lat=%lf lon=%lf hDOP=%lf hAcc=%lf numSV=%u batt=%lfV ", gps_entry.info.lat, gps_entry.info.lon,
    		static_cast<double>(gps_entry.info.hDOP),
			static_cast<double>(gps_entry.info.hAcc),
			gps_entry.info.numSV,
			(double)gps_entry.info.batt_voltage / 1000);

    m_is_active = false;
    m_device.power_off();
    m_gnss_data.pending_data_logging = false;

    // Notify configuration store that we have a new valid GPS fix
    configuration_store->notify_gps_location(gps_entry);

    ServiceEventData event_data = gps_entry;
    service_complete(&event_data, &gps_entry);
}

void GPSService::react(const GPSEventPowerOff& e) {
	if (!m_is_active)
		return;
    DEBUG_TRACE("GPSService::react(GPSEventPowerOff)");
    m_is_active = false;
    m_device.power_off();
    if (!e.fix_found) {
		GPSLogEntry log_entry = invalid_log_entry();
		ServiceEventData event_data = log_entry;
		service_complete(&event_data, &log_entry);
    }
}

void GPSService::react(const GPSEventError&) {
}

void GPSService::react(const GPSEventPVT& e) {
	if (!m_is_active)
		return;
    gnss_data_callback(e.data);
}

void GPSService::gnss_data_callback(GNSSData data) {
    // If we haven't finished processing our last data then ignore this one
    if (m_gnss_data.pending_data_logging)
    	return;

    // Mark first fix flag
    m_gnss_data.data = data;
    m_is_first_fix_found = true;
    m_num_gps_fixes++;

    {
        // Defer processing this data till we are outside of this interrupt context
        m_gnss_data.pending_data_logging = true;
        m_task_process_gnss_data = system_scheduler->post_task_prio(
        		[this]() { task_process_gnss_data(); },
        		"GPSProcessGNSSData",
        		Scheduler::DEFAULT_PRIORITY);
    }
}

void GPSService::populate_gps_log_with_time(GPSLogEntry &entry, std::time_t time)
{
	service_set_log_header_time(entry.header, time);
}

bool GPSService::service_is_triggered_on_event(ServiceEvent& event, bool& immediate) {
	if (event.event_source == ServiceIdentifier::AXL_SENSOR &&
			event.event_type == ServiceEventType::SERVICE_LOG_UPDATED &&
			std::get<bool>(event.event_data)) {
		bool trigger_on_axl = service_read_param<bool>(ParamID::GNSS_TRIGGER_ON_AXL_WAKEUP);
		immediate = trigger_on_axl;
		return trigger_on_axl;
	}

	return false;
}
