#pragma once

#include <functional>
#include <variant>

enum class ServiceEventType {
	SERVICE_ACTIVE,
	SERVICE_INACTIVE,
	SERVICE_LOG_UPDATED,
	// Legacy events
	GNSS_ON,
	ARGOS_TX_START,
	ARGOS_TX_END,
	SENSOR_LOG_UPDATED
};

using ServiceEventData = std::variant<bool>;

enum class ServiceIdentifier : unsigned int {
	ARGOS_TX,
	ARGOS_RX,
	GNSS_SENSOR,
	CDT_SENSOR,
	PRESSURE_SENSOR,
	UW_SENSOR,
	ALS_SENSOR,
	AIR_TEMP_SENSOR,
	SEA_TEMP_SENSOR,
	PH_SENSOR
};

struct ServiceEvent {
	ServiceEventType  event_type;
	ServiceEventData  event_data;
	ServiceIdentifier event_source;
};

// Legacy code for old interface

class ServiceScheduler {
public:
	virtual ~ServiceScheduler() {}
	virtual void start(std::function<void(ServiceEvent&)> data_notification_callback = nullptr) = 0;
	virtual void stop() = 0;
	virtual void notify_underwater_state(bool state) = 0;
	virtual void notify_sensor_log_update() = 0;
};
