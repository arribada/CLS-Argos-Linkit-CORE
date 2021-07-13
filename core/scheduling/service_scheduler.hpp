#pragma once

#include <functional>
#include <variant>

enum class ServiceEventType {
	GNSS_ON,
	SENSOR_LOG_UPDATED,
	ARGOS_TX_START,
	ARGOS_TX_END
};

using ServiceEventData = std::variant<bool>;

struct ServiceEvent {
	ServiceEventType event_type;
	ServiceEventData event_data;
};


class ServiceScheduler {
public:
	virtual ~ServiceScheduler() {}
	virtual void start(std::function<void(ServiceEvent&)> data_notification_callback = nullptr) = 0;
	virtual void stop() = 0;
	virtual void notify_saltwater_switch_state(bool state) = 0;
	virtual void notify_sensor_log_update() = 0;
};
