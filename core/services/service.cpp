#include "service.hpp"
#include "scheduler.hpp"
#include "rtc.hpp"


extern Scheduler *system_scheduler;
extern RTC *rtc;


unsigned int ServiceManager::add(Service& s) {
	m_map.insert({m_unique_identifier, s});
	return m_unique_identifier++;
}

void ServiceManager::remove(Service& s) {
	m_map.erase(s.get_unique_id());
}


void ServiceManager::startall(std::function<void(ServiceEvent&)> data_notification_callback) {
	for (auto const& p : m_map)
		p.second.start(data_notification_callback);
}

void ServiceManager::stopall() {
	for (auto const& p : m_map)
		p.second.stop();
}

void ServiceManager::notify_underwater_state(bool state) {
	for (auto const& p : m_map)
		p.second.notify_underwater_state(state);
}

void ServiceManager::notify_peer_event(ServiceEvent& event) {
	for (auto const& p : m_map) {
		if (p.second.get_service_id() != event.event_source)
			p.second.notify_peer_event(event);
	}
}

Logger *ServiceManager::get_logger(ServiceIdentifier service_id) {
	for (auto const& p : m_map) {
		if (p.second.get_service_id() == service_id && p.second.get_logger())
			return p.second.get_logger();
	}

	return nullptr;
}

Service::Service(ServiceIdentifier service_id, const char *name, Logger *logger) {
	m_name = name;
	m_is_underwater = false;
	m_service_id = service_id;
	m_logger = logger;
	m_unique_id = ServiceManager::add(*this);
}

Service::~Service() {
	ServiceManager::remove(*this);
}

unsigned int Service::get_unique_id() { return m_unique_id; }
ServiceIdentifier Service::get_service_id() { return m_service_id; }
Logger *Service::get_logger() { return m_logger; }

void Service::start(std::function<void(ServiceEvent&)> data_notification_callback) {
	DEBUG_TRACE("Service::start: service %s started", m_name);
	m_data_notification_callback = data_notification_callback;
	service_init();
	reschedule();
}

void Service::stop() {
	DEBUG_TRACE("Service::start: service %s stopped", m_name);
	deschedule();
	if (service_cancel())
		notify_service_inactive();
	service_term();
}

void Service::notify_underwater_state(bool state) {
	if (service_is_usable_underwater())
		return; // Don't care since the sensor can be used underwater
	m_is_underwater = state;
	if (m_is_underwater) {
		if (service_cancel()) {
			notify_service_inactive();
			reschedule();
		}
	} else {
		if (service_is_triggered_on_surfaced())
			reschedule(true);
	}
}

// May also be overridden in child class to receive peer service events
void Service::notify_peer_event(ServiceEvent& event) {
	if (event.event_source == ServiceIdentifier::UW_SENSOR)
		notify_underwater_state(std::get<bool>(event.event_data));
};

void Service::service_complete(ServiceEventData *event_data, void *entry) {
	DEBUG_TRACE("Service::service_complete: service %s", m_name);
	if (m_logger && entry != nullptr)
		m_logger->write(entry);
	if (event_data)
		notify_log_updated(*event_data);
	reschedule();
}

void Service::service_set_log_header_time(LogHeader& header, std::time_t time)
{
    uint16_t year;
    uint8_t month, day, hour, min, sec;

    convert_datetime_to_epoch(time, year, month, day, hour, min, sec);

    header.year = year;
    header.month = month;
    header.day = day;
    header.hours = hour;
    header.minutes = min;
    header.seconds = sec;
}

std::time_t Service::service_current_time() {
	return rtc->gettime();
}

void Service::reschedule(bool immediate) {
	DEBUG_TRACE("Service::reschedule: service %s", m_name);
	deschedule();
	if (service_is_enabled()) {
		unsigned int next_schedule = immediate ? 0 : service_next_schedule_in_ms();

		if (next_schedule != SCHEDULE_DISABLED) {
			DEBUG_TRACE("Service::reschedule: service %s scheduled in %u msecs", m_name, next_schedule);
			m_task_period = system_scheduler->post_task_prio(
				[this]() {
				unsigned int timeout_ms = service_next_timeout();
				if (timeout_ms) {
					m_task_timeout = system_scheduler->post_task_prio(
						[this]() {
						DEBUG_TRACE("Service::reschedule: service %s timed out", m_name);
						if (service_cancel())
							notify_service_inactive();
						reschedule();
					}, "ServiceTimeoutPeriod", Scheduler::DEFAULT_PRIORITY, timeout_ms);
				}

				if (!m_is_underwater) {
					DEBUG_TRACE("Service::reschedule: service %s active", m_name);
					notify_service_active();
					service_initiate();
				} else {
					DEBUG_TRACE("Service::reschedule: service %s can't run underwater, rescheduling", m_name);
					reschedule();
				}
			}, "ServicePeriod", Scheduler::DEFAULT_PRIORITY, next_schedule);
		} else {
			DEBUG_TRACE("Service::reschedule: service %s schedule currently disabled", m_name);
		}
	} else {
		DEBUG_TRACE("Service::reschedule: service %s is not enabled", m_name);
	}
}

void Service::deschedule() {
	system_scheduler->cancel_task(m_task_timeout);
	system_scheduler->cancel_task(m_task_period);
}

void Service::notify_log_updated(ServiceEventData& data) {
	if (m_data_notification_callback) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_LOG_UPDATED;
		e.event_source = m_service_id;
		e.event_data = data;
		m_data_notification_callback(e);
	}
}

void Service::notify_service_active() {
	if (m_data_notification_callback) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_ACTIVE;
		e.event_source = m_service_id;
		m_data_notification_callback(e);
	}
}

void Service::notify_service_inactive() {
	if (m_data_notification_callback) {
		ServiceEvent e;
		e.event_type = ServiceEventType::SERVICE_INACTIVE;
		e.event_source = m_service_id;
		m_data_notification_callback(e);
	}
}
