#include "cam_service.hpp"
#include "config_store.hpp"
#include "scheduler.hpp"

extern ConfigurationStore *configuration_store;
extern Scheduler *system_scheduler;


#define MS_PER_SEC         (1000)

void CAMService::service_init() {
	m_is_active = false;
    m_num_captures = 0;
}

void CAMService::service_term() {
}

bool CAMService::service_is_enabled() {
	return service_read_param<bool>(ParamID::CAM_ENABLE);
}

unsigned int CAMService::service_next_schedule_in_ms() {
    std::time_t now = service_current_time();
    std::time_t period_on = service_read_param<unsigned int>(ParamID::CAM_PERIOD_ON);
    std::time_t period_off = service_read_param<unsigned int>(ParamID::CAM_PERIOD_ON);
    std::time_t aq_period = period_on + period_off;

    if (period_on == 0) {
    	return Service::SCHEDULE_DISABLED;
    }

    // Find the next schedule time aligned to UTC 00:00
    std::time_t next_schedule = now - (now % (aq_period)) + aq_period - period_off;

    DEBUG_TRACE("CAMService::reschedule: period_on=%u period_off=%u now=%u next=%u",
    		(unsigned int)period_on, (unsigned int)period_off,
			(unsigned int)now, (unsigned int)next_schedule);

    // Find the time in milliseconds until this schedule
    return (next_schedule - now) * MS_PER_SEC;
}

void CAMService::service_initiate() {

	m_next_schedule = service_current_time();
	m_wakeup_time = service_current_timer();
	m_is_active = true;
	m_device.power_on();
}

bool CAMService::service_cancel() {
	// Cleanly terminate
	DEBUG_TRACE("CAMService::service_cancel");

	if (m_is_active) {
		m_is_active = false;
		m_device.power_off();
		CAMLogEntry log_entry = invalid_log_entry();
		ServiceEventData event_data = log_entry;
		service_complete(&event_data, &log_entry);
		return true;
	}

	return false;
}

unsigned int CAMService::service_next_timeout() {
	// No timeoute managed
	return 0;
}

bool CAMService::service_is_triggered_on_surfaced(bool& immediate) {
    return service_read_param<bool>(ParamID::CAM_TRIGGER_ON_SURFACED);
}

bool CAMService::service_is_usable_underwater() {
	return true;
}

CAMLogEntry CAMService::invalid_log_entry()
{
    DEBUG_INFO("CAMService::invalid_log_entry");

    CAMLogEntry cam_entry;
    memset(&cam_entry, 0, sizeof(cam_entry));

    cam_entry.header.log_type = LOG_CAM;

    populate_cam_log_with_time(cam_entry, service_current_time());

	service_update_battery();
    cam_entry.info.batt_voltage = service_get_voltage();
    cam_entry.info.event_type = CAMEventType::OFF;
    cam_entry.info.schedTime = m_next_schedule;

    return cam_entry;
}

void CAMService::task_process_cam_data()
{
    DEBUG_TRACE("CAMService::task_process_cam_data");

    CAMLogEntry cam_entry;
    memset(&cam_entry, 0, sizeof(cam_entry));

    cam_entry.header.log_type = LOG_CAM;

    populate_cam_log_with_time(cam_entry, service_current_time());

	service_update_battery();
    cam_entry.info.batt_voltage = service_get_voltage();

    cam_entry.info.schedTime     = m_next_schedule;

    cam_entry.info.event_type = CAMEventType::ON;
    cam_entry.info.counter = m_num_captures;

    DEBUG_INFO("CAMService::task_process_cam_data: batt=%lfV state=%u count=%u", 
			(double)cam_entry.info.batt_voltage / 1000,
            (unsigned int)cam_entry.info.event_type,
            (unsigned int)cam_entry.info.counter
            );

    // Notify configuration store that we have a new valid GPS fix
    //configuration_store->notify_gps_location(gps_entry);

    ServiceEventData event_data = cam_entry;
    service_complete(&event_data, &cam_entry);
}

void CAMService::react(const CAMEventError&) {
	if (!m_is_active)
		return;
    DEBUG_TRACE("CAMService::react(CAMEventError)");
    m_device.power_off();
    CAMLogEntry log_entry = invalid_log_entry();
    ServiceEventData event_data = log_entry;
    service_complete(&event_data, &log_entry);
}

void CAMService::cam_data_callback() {
    // Mark first fix flag
	m_num_captures++;
    task_process_cam_data();
}

void CAMService::populate_cam_log_with_time(CAMLogEntry &entry, std::time_t time)
{
	service_set_log_header_time(entry.header, time);
}

bool CAMService::service_is_triggered_on_event(ServiceEvent& event, bool& immediate) {
	if (event.event_source == ServiceIdentifier::AXL_SENSOR &&
			event.event_type == ServiceEventType::SERVICE_LOG_UPDATED &&
			std::get<bool>(event.event_data)) {
		bool trigger_on_axl = service_read_param<bool>(ParamID::CAM_TRIGGER_ON_AXL_WAKEUP);
		immediate = trigger_on_axl;
		return trigger_on_axl;
	}

	return false;
}

void CAMService::notify_peer_event(ServiceEvent& e) {
	//DEBUG_TRACE("GPSService::notify_peer_event: (%u,%u)", e.event_source, e.event_type);
	Service::notify_peer_event(e);
}
