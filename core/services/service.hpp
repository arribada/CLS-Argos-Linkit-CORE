#pragma once

#include <functional>
#include <variant>
#include <ctime>

#include "service_scheduler.hpp"
#include "scheduler.hpp"
#include "logger.hpp"
#include "base_types.hpp"
#include "config_store.hpp"

extern ConfigurationStore *configuration_store;

class Service {
public:
	static inline const unsigned int SCHEDULE_DISABLED = 0xFFFFFFFF;

	Service(ServiceIdentifier service_id, const char *name, Logger *logger = nullptr);
	virtual ~Service();
	unsigned int get_unique_id();
	ServiceIdentifier get_service_id();
	Logger *get_logger();
	void set_logger(Logger *logger);
	void start(std::function<void(ServiceEvent&)> data_notification_callback = nullptr);
	void stop();
	virtual void notify_peer_event(ServiceEvent& event);
	bool is_started();
	unsigned int get_last_schedule();

private:
	bool m_is_started;
	const char *m_name;
	bool m_is_underwater;
	bool m_is_initiated;
	bool m_is_scheduled;
	Scheduler::TaskHandle m_task_period;
	Scheduler::TaskHandle m_task_timeout;
	std::function<void(ServiceEvent&)> m_data_notification_callback;
	ServiceIdentifier m_service_id;
	unsigned int m_unique_id;
	Logger *m_logger;
	unsigned int m_last_schedule;

	void reschedule(bool immediate = false);
	void deschedule();
	void notify_log_updated(ServiceEventData& data);
	void notify_service_active();
	void notify_service_inactive();
	void notify_underwater_state(bool state);

protected:
	// This interface must be implemented by the underlying sensor logic
	virtual void service_init() = 0;
	virtual void service_term() = 0;
	virtual bool service_is_enabled() = 0;
	virtual unsigned int service_next_schedule_in_ms() = 0;
	virtual void service_initiate() = 0;
	virtual bool service_cancel() { return false; }
	virtual unsigned int service_next_timeout() { return 0; }
	virtual bool service_is_triggered_on_surfaced(bool&) { return false; }
	virtual bool service_is_usable_underwater() { return false; }
	virtual bool service_is_triggered_on_event(ServiceEvent&, bool&) { return false; }

	// This can be called by underlying service
	bool service_is_scheduled();
	void service_reschedule(bool immediate = false);
	void service_complete(ServiceEventData *event_data = nullptr, void *entry = nullptr, bool reschedule = true);
	void service_set_log_header_time(LogHeader& header, std::time_t time);
	std::time_t service_current_time();
	void service_set_time(std::time_t);
	uint16_t service_get_voltage();
	bool service_is_battery_level_low();
	uint64_t service_current_timer();
	template <typename T> T& service_read_param(ParamID param_id) {
		return configuration_store->read_param<T>(param_id);
	}
	template <typename T> void service_write_param(ParamID param_id, T& value) {
		configuration_store->write_param(param_id, value);
	}
};

class ServiceManager
{
private:
	static inline std::function<void(ServiceEvent&)> m_data_notification_callback = nullptr;
	static inline unsigned int m_unique_identifier = 0;
	static inline std::map<unsigned int, Service&> m_map;

public:
	static unsigned int add(Service& s);
	static void remove(Service& s);
	static void startall(std::function<void(ServiceEvent&)> data_notification_callback = nullptr);
	static void stopall();
	static void notify_underwater_state(bool state);
	static void notify_peer_event(ServiceEvent& event);
	static void inject_event(ServiceEvent& event);
	static Logger *get_logger(ServiceIdentifier service_id);
};
