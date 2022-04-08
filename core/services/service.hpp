#pragma once

#include <functional>
#include <variant>
#include <ctime>

#include "service_scheduler.hpp"
#include "scheduler.hpp"
#include "logger.hpp"


class Service {
public:
	static inline const unsigned int SCHEDULE_DISABLED = 0xFFFFFFFF;

	Service(ServiceIdentifier service_id, const char *name, Logger *logger = nullptr);
	virtual ~Service();
	unsigned int get_unique_id();
	ServiceIdentifier get_service_id();
	Logger *get_logger();
	void start(std::function<void(ServiceEvent&)> data_notification_callback = nullptr);
	void stop();
	void notify_underwater_state(bool state);
	void notify_peer_event(ServiceEvent& event);

private:
	const char *m_name;
	bool m_is_underwater;
	Scheduler::TaskHandle m_task_period;
	Scheduler::TaskHandle m_task_timeout;
	std::function<void(ServiceEvent&)> m_data_notification_callback;
	ServiceIdentifier m_service_id;
	unsigned int m_unique_id;
	Logger *m_logger;

	void reschedule(bool immediate = false);
	void deschedule();
	void notify_log_updated(ServiceEventData& data);
	void notify_service_active();
	void notify_service_inactive();

protected:
	// This interface must be implemented by the underlying sensor logic
	virtual void service_init() = 0;
	virtual void service_term() = 0;
	virtual bool service_is_enabled() = 0;
	virtual unsigned int service_next_schedule_in_ms() = 0;
	virtual void service_initiate() = 0;
	virtual bool service_cancel() = 0;
	virtual unsigned int service_next_timeout() = 0;
	virtual bool service_is_triggered_on_surfaced() = 0;
	virtual bool service_is_usable_underwater() = 0;

	// This can be called by underlying service
	void service_complete(ServiceEventData *event_data = nullptr, void *entry = nullptr);
	void service_set_log_header_time(LogHeader& header, std::time_t time);
	std::time_t service_current_time();
};

class ServiceManager
{
private:
	static inline unsigned int m_unique_identifier = 0;
	static inline std::map<unsigned int, Service&> m_map;

public:
	static unsigned int add(Service& s);
	static void remove(Service& s);
	static void startall(std::function<void(ServiceEvent&)> data_notification_callback = nullptr);
	static void stopall();
	static void notify_underwater_state(bool state);
	static void notify_peer_event(ServiceEvent& event);
	static Logger *get_logger(ServiceIdentifier service_id);
};
